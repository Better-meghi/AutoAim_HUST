#include "dynamicdraw.h"

DynamicDraw::DynamicDraw(QCustomPlot* customPlot,QObject *parent)
    : QObject(parent),m_draw(customPlot),m_bufferSize(0),m_magnification(0.2),m_max(0),m_min(0),m_drawBuffer(NULL)
{
    if(!lcm.good())
    {
        qDebug()<<"CRETE LCM MODULE ERROR"<<"\n";
    }
    lcm.subscribe("debug",&DynamicDraw::handle_function,this);
    connect(&m_lineTimer, &QTimer::timeout, this, &DynamicDraw::on_refreshLine_timeOut);
    connect(&m_axisTimer, &QTimer::timeout, this, &DynamicDraw::on_refreshAxis_timeOut);
    connect(&m_communicateTimer,&QTimer::timeout,this,&DynamicDraw::on_refrashData_timeout);
}

DynamicDraw::~DynamicDraw()
{

}

void DynamicDraw::allotBuffer(u_short size, float initialValue)
{
    //绘制一条曲线
    while (m_draw->removeGraph(0));
    m_bufferSize = size;
    QSharedPointer<QCPGraphDataContainer> dataContainer = m_draw->addGraph()->data();
    QVector<QCPGraphData> plotData(m_bufferSize);
    for (size_t j = 0; j < m_bufferSize; j++)
    {
        plotData[j].key = j;
        plotData[j].value = initialValue;
    }
    dataContainer->set(plotData, true);
    m_draw->xAxis->setRange(0, m_bufferSize);
    m_dataQueue.clear();
    m_drawBuffer = m_draw->graph(0)->data().data();

    //绘制另一条曲线
    QSharedPointer<QCPGraphDataContainer> dataContainer2 = m_draw->addGraph()->data();
    m_draw->graph(1)->setPen(QPen(Qt::red));
    QVector<QCPGraphData> plotData2(m_bufferSize);
    for (size_t j = 0; j < m_bufferSize; j++)
    {
        plotData2[j].key = j;
        plotData2[j].value = initialValue;
    }
    dataContainer2->set(plotData2,true);
    m_dataQueue2.clear();
    m_drawBuffer2 = m_draw->graph(1)->data().data();
}

void DynamicDraw::addData(float data)
{
    if (m_dataQueue.size() >= m_bufferSize)
        m_dataQueue.dequeue();
    m_dataQueue.enqueue(data);
}
void DynamicDraw::addData2(float data)
{
    if (m_dataQueue2.size() >= m_bufferSize)
        m_dataQueue2.dequeue();
    m_dataQueue2.enqueue(data);
}

void DynamicDraw::startDraw(u_short lineRefreshTime, u_short axisRefreshTime)
{
    if (m_lineTimer.isActive())
        m_lineTimer.stop();
    if (m_axisTimer.isActive())
        m_axisTimer.stop();
    if (m_communicateTimer.isActive())
        m_communicateTimer.stop();
    m_lineTimer.start(lineRefreshTime);
    m_axisTimer.start(axisRefreshTime);
    m_communicateTimer.start(12);
}

void DynamicDraw::magnify()
{
    if (m_magnification > 0.1)
        m_magnification -= 0.1;
}

void DynamicDraw::shrink()
{
    if (m_magnification < 5)
        m_magnification += 0.1;
}

void DynamicDraw::on_refreshLine_timeOut()  //更新数据
{
    size_t size = m_dataQueue.size();
    for (size_t j = size - 1; j; --j)
    {
        QCPGraphData* buff = (QCPGraphData*)m_drawBuffer->at(j);
        buff->value = m_dataQueue.at(j);

        QCPGraphData* buff2 = (QCPGraphData*)m_drawBuffer2->at(j);
        buff2->value = m_dataQueue2.at(j);
    }
    m_draw->replot(QCustomPlot::rpQueuedReplot);
}

void DynamicDraw::on_refreshAxis_timeOut()  //设置轴信息
{
    m_max = *std::max_element(m_dataQueue.begin(), m_dataQueue.end());
    m_min = *std::min_element(m_dataQueue.begin(), m_dataQueue.end());
    double m_max2 = *std::max_element(m_dataQueue2.begin(), m_dataQueue2.end());
    double m_min2 = *std::min_element(m_dataQueue2.begin(), m_dataQueue2.end());
    m_max = m_max>m_max2?m_max:m_max2;
    m_min = m_min<m_min2?m_min:m_min2;
    m_draw->xAxis->setRange(0, m_bufferSize);
    if (m_max < 0)
        m_draw->yAxis->setRange(m_min + m_min * m_magnification, m_max - m_max * m_magnification);
    else
    {
        if (m_min > 0)
            m_draw->yAxis->setRange(m_min - m_min * m_magnification, m_max + m_max * m_magnification);
        else
            m_draw->yAxis->setRange(m_min + m_min * m_magnification, m_max + m_max * m_magnification);
    }
}

void DynamicDraw::on_refrashData_timeout()
{
    lcm.handle();
}
void DynamicDraw::handle_function(const lcm::ReceiveBuffer* rbuf,
                                  const std::string& chan,
                                  const exlcm::example_t* msg)
{
    this->addData(msg->speed[0]);
    this->addData2(0);
    //this->addData(msg->kf_pose[2]);

    //this->addData2(msg->kf_speed[0]);
    //this->addData(sqrt(msg->speed[0] * msg->speed[0] + msg->speed[1] * msg->speed[1]));
    //this->addData2(msg->speed[1]);
    //this->addData(msg->pitch[0]);
    //this->addData2(msg->pitch[1]);
    //this->addData(msg->yaw[0]);
    //this->addData2(msg->yaw[1]);
    //qDebug()<<"add data "<<endl;
}
