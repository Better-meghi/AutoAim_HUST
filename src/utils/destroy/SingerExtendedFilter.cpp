#include "SingerExtendedFilter.h"

namespace ly
{
    SingerExtendedFilter::SingerExtendedFilter(/* args */)
    {
        is_kalman_init = false;
        kalman_filter = new ExtendedKalman<double, 9, 3>();
        posteriori_pose = Eigen::Vector3d::Zero();
        posteriori_speed = Eigen::Vector3d::Zero();
        posteriori_accelerate = Eigen::Vector3d::Zero();
        setMeasurementNoise();
        alpha = FilterParams::alpha;
        setMaxA();
    }

    SingerExtendedFilter::~SingerExtendedFilter()
    {
    }
    void SingerExtendedFilter::setMaxA()
    {
        a_max[0] = FilterParams::max_a_x;
        a_max[1] = FilterParams::max_a_y;
        a_max[2] = FilterParams::max_a_z;
    }
    void SingerExtendedFilter::rebootKalman(const Eigen::Vector3d &new_armor_pose)
    {
        for (int i = 0; i < 3; i++)
        {
            kalman_filter->posteriori_state_estimate[i * 3] = new_armor_pose[i];
            kalman_filter->posteriori_state_estimate[i * 3 + 1] = 0;
            kalman_filter->posteriori_state_estimate[i * 3 + 2] = 0;
        }
        kalman_filter->error_cov_post = Eigen::Matrix<double, 9, 9>::Identity();
        resetTransitionMatrix();
    }
    void SingerExtendedFilter::resetTransitionMatrix()
    {
        kalman_filter->transition_matrix = Eigen::Matrix<double, 9, 9>::Identity();
    }
    void SingerExtendedFilter::setUpdateTime(const double &delta_t)
    {
        if (fabs(delta_t) < 1e-4) //防止时间差为0
        {
            update_time = 8.0 / 1000.0;
        }
        else
        {
            update_time = delta_t / 1000.0;
        }
    }
    Eigen::Vector3d SingerExtendedFilter::runKalman(const Eigen::Vector3d &new_armor_pose, const double &delta_t)
    {
        if (!is_kalman_init)
        {
            //set signal values
            is_kalman_init = true;

            //reset kalman
            rebootKalman(new_armor_pose);

            //return values
            return new_armor_pose;
        }
        else
        {
            //set update time
            setUpdateTime(delta_t);

            //update transition matrix
            setTransitionMatrix();

            calSigma();
            setControlVector();
            setProcessNoise();

            return correct(new_armor_pose);
        }
    }
    void SingerExtendedFilter::setTransitionMatrix()
    {
        Eigen::Matrix3d transition;
        double F0_1 = update_time; //s
        double F2_2 = exp(-F0_1 * alpha);
        double F1_2 = (1 - F2_2) / alpha;
        double F0_2 = (F2_2 + alpha * F0_1 - 1) / alpha / alpha;
        Eigen::Matrix3d descript_matrix;
        descript_matrix << 1, F0_1, F0_2,
            0, 1, F1_2,
            0, 0, F2_2;
        for (int i = 0; i < 9; i += 3)
        {
            kalman_filter->transition_matrix.block<3, 3>(i, i) = descript_matrix;
        }
    }
    void SingerExtendedFilter::setControlVector()
    {
        Eigen::Matrix<double, 3, 1> control_matrix;
        double T = update_time;
        double exp_alpha = exp(-alpha * T);
        control_matrix[0] = 1 / alpha * (-T + alpha * T * T / 2 + (1 - exp_alpha) / alpha);
        control_matrix[1] = T - (1 - exp_alpha) / alpha;
        control_matrix[2] = 1 - exp_alpha;
        kalman_filter->control_matrix = Eigen::Matrix<double, 9, 9>::Identity();
        for (int i = 0; i < 3; i++)
        {
            kalman_filter->control_vector.block<3, 1>(i * 3, 0) = control_matrix * posteriori_accelerate[i];
        }
    }
    void SingerExtendedFilter::calSigma()
    {
        for (int i = 0; i < 3; i++) //使用a_max，这个在correct前
        {
            sigma_2[i] = (4 / M_PI - 1) * pow((a_max[i] - fabs(posteriori_accelerate[i])), 2);
        }
        // for (int i = 0; i < 3; i++) //不使用a_max，注意在主程序中两个放置的地方不同，在correct后
        // {
        //     sigma_2[i] = (4 / M_PI - 1) * pow((a_max[i] + 2 / update_time / update_time * (posteriori_pose[i] - last_posteriori_pose[i])), 2);
        // }
    }

    Eigen::Vector3d SingerExtendedFilter::measure(const Eigen::Vector3d &armor_pose)
    {
        Eigen::Vector3d pyd;
        pyd[2] = sqrt(armor_pose[0] * armor_pose[0] + armor_pose[1] * armor_pose[1]);
        pyd[0] = armor_pose[2] / pyd[2]; //pitch
        pyd[1] = -armor_pose[0] / armor_pose[1];
        return pyd;
    }
    Eigen::Vector3d SingerExtendedFilter::correct(const Eigen::Vector3d &armor_pose)
    {
        Eigen::Vector3d pyd = measure(armor_pose);
        // kalman_filter->predict();

        if (!kalman_filter->ChiSquaredTest()) //true则卡方检验通过
        {
            std::cout << " CHI SQUARED TEST NOT PASS" << std::endl;

            rebootKalman(armor_pose); //重启卡尔曼滤波器
            return armor_pose;
        }
        // kalman_filter->update(tool, pyd); //量测有效更新
        for (int i = 0; i < 3; i++)
        {
            //update armor status and return
            posteriori_pose[i] = kalman_filter->posteriori_state_estimate[i * 3];
            posteriori_speed[i] = kalman_filter->posteriori_state_estimate[i * 3 + 1];
            posteriori_accelerate[i] = kalman_filter->posteriori_state_estimate[i * 3 + 2];
        }
        return posteriori_pose;
    }
    void SingerExtendedFilter::setMeasurementNoise()
    {
        //注意这里pyd和xyz噪声一致，便于在不同滤波器之间调试
        double position_cov_p = FilterParams::measurement_noise_pose_x;
        double position_cov_y = FilterParams::measurement_noise_pose_y;
        double position_cov_d = FilterParams::measurement_noise_pose_z;

        kalman_filter->measurement_noise_cov.diagonal() << position_cov_p, position_cov_y, position_cov_d;
    }
    void SingerExtendedFilter::setProcessNoise()
    {
        double T = update_time;
        double alpha_5 = pow(alpha, 5);
        double alpha_3 = pow(alpha, 3);
        double alpha_2 = pow(alpha, 2);
        double alpha_4 = pow(alpha, 4);
        double T_2 = pow(T, 2);
        double T_3 = pow(T, 3);
        double e_1_alpha_t = exp(-alpha * T);
        double e_2_alpha_t = e_1_alpha_t * e_1_alpha_t;
        double rho_00 = 1 / (2 * alpha_5) * (1 - e_2_alpha_t + 2 * alpha * T + 2 * alpha_3 * T_3 / 3 - 2 * alpha_2 * T_2 - 4 * alpha * T * e_1_alpha_t);
        double rho_01 = 1 / (2 * alpha_4) * (1 + e_2_alpha_t - 2 * alpha * T - 2 * e_1_alpha_t + alpha_2 * T_2 + 2 * alpha * T * e_1_alpha_t);
        double rho_02 = 1 / (2 * alpha_3) * (1 - e_2_alpha_t - 2 * alpha * T * e_1_alpha_t);
        double rho_11 = 1 / (2 * alpha_3) * (-3 - e_2_alpha_t + 2 * alpha * T + 4 * e_1_alpha_t);
        double rho_21 = 1 / (2 * alpha_2) * (1 + e_2_alpha_t - 2 * e_1_alpha_t);
        double rho_22 = 1 / (2 * alpha) * (1 - e_2_alpha_t);

        Eigen::Matrix3d Q;
        Q << rho_00, rho_01, rho_02, rho_01, rho_11, rho_21, rho_02, rho_21, rho_22;

        for (int i = 0; i < 3; i++)
        {
            kalman_filter->process_noise_cov.block<3, 3>(i * 3, i * 3) = Q * 2 * alpha * sigma_2[i];
        }
    }
    Eigen::Vector3d SingerExtendedFilter::predict(const double &predict_t)
    {
        // return posteriori_pose + posteriori_speed * predict_t;
        return posteriori_pose + posteriori_speed * predict_t + 0.5 * predict_t * predict_t * posteriori_accelerate;
    }
}