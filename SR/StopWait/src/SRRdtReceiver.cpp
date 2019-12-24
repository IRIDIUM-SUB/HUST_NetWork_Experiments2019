#include "SRRdtReceiver.h"
#include "Global.h"

void SRRdtReceiver::Init()
{
	base = 0;
	//nextSeqnum = 0;
	for (int i = 0; i < seqsize; i++)
	{
		bufStatus[i] = false;
	}
	lastAckPkt.acknum = -1; //初始状态下，上次发送的确认包的确认序号为0，使得当第一个接受的数据包出错时该确认报文的确认号为0
	lastAckPkt.checksum = 0;
	lastAckPkt.seqnum = -1;	//忽略该字段
	memset(lastAckPkt.payload, '.', Configuration::PAYLOAD_SIZE);
	lastAckPkt.checksum = pUtils->calculateCheckSum(lastAckPkt);
}

void SRRdtReceiver::printSlideWindow()
{
	int i;
	for (i = 0; i < seqsize; i++)
	{
		if (i == base)
			std::cout << "[";
		if (isInWindow(i) && bufStatus[i] == true)
			std::cout << "Saved";
		else if (isInWindow(i))
			std::cout << "Expected";
		if (i == (base + wndsize) % seqsize)
			std::cout << "]";
		if (isInWindow(i) == false)
			std::cout << "Unavaliable";
		std::cout << " ";
	}
	std::cout << std::endl;
}

bool SRRdtReceiver::isInWindow(int seqnum)
{
	//return seqnum >= base && seqnum <= (base + wndsize) % seqsize;
	if (base < (base + wndsize) % seqsize)
	{
		return seqnum >= base && seqnum < (base + wndsize) % seqsize;
	}
	else
	{
		return seqnum >= base || seqnum < (base + wndsize) % seqsize;
	}
}

SRRdtReceiver::SRRdtReceiver():
	seqsize(8),wndsize(4),recvBuf(new Packet[seqsize]),bufStatus(new bool[seqsize])
{
	Init();
}

SRRdtReceiver::SRRdtReceiver(int sSize, int wsize):
	seqsize(sSize),wndsize(wsize), recvBuf(new Packet[seqsize]), bufStatus(new bool[seqsize])
{
	Init();
}

void SRRdtReceiver::receive(Packet & packet)
{
	int checksum = pUtils->calculateCheckSum(packet);
	if (checksum != packet.checksum)
	{//数据包损坏，不作出应答
		pUtils->printPacket("[Debug]Checksum Error", packet);
		//std::cout << "\n\n接收的数据分组校验和错误\n\n";
		return;
	}
	else
	{
		if (isInWindow(packet.seqnum) == false)
		{//不是想要的数据包，不作出应答
			pUtils->printPacket("[Debug]Not expected,skip", packet);
			//std::cout << "\n\n不是期望的数据分组\n\n";
			//lastAckPkt.acknum = base;
			lastAckPkt.acknum = packet.seqnum;
			lastAckPkt.seqnum = -1;//无用
			memset(lastAckPkt.payload, '.', Configuration::PAYLOAD_SIZE);
			lastAckPkt.checksum = pUtils->calculateCheckSum(lastAckPkt);
			pns->sendToNetworkLayer(SENDER, lastAckPkt);
			return;
		}
		else
		{//是自己想要的数据包，发送ack，更新缓冲区和滑动窗口
			recvBuf[packet.seqnum] = packet;//Packet类重载了赋值运算符
			bufStatus[packet.seqnum] = true;
			lastAckPkt.acknum = packet.seqnum;
			lastAckPkt.seqnum = 0;//无用
			memset(lastAckPkt.payload, '.', sizeof(lastAckPkt.payload));//无用
			pUtils->printPacket("[Debug]Getting ACK", lastAckPkt);
			pns->sendToNetworkLayer(SENDER, lastAckPkt);
			while (bufStatus[base] == true)
			{
				Message msg;
				memcpy(msg.data, recvBuf[base].payload, sizeof(recvBuf[base].payload));
				pns->delivertoAppLayer(RECEIVER, msg);
				pUtils->printPacket("[Debug]Delivering to the upper layer:", recvBuf[base]);
				bufStatus[base] = false;
				base = (base + 1) % seqsize;
			}
			std::cout << "\n[RECEIVER]Receive and Move window：";
			printSlideWindow();
			std::cout << std::endl;
		}
	}

}


SRRdtReceiver::~SRRdtReceiver()
{
	delete[] recvBuf;
	delete[] bufStatus;
}