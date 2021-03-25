#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ex5");

int
main (int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_PREFIX_TIME);
    LogComponentEnable("UdpEchoServerApplication", LOG_PREFIX_TIME);
    LogComponentEnable("UdpEchoClientApplication", LOG_PREFIX_FUNC);
    LogComponentEnable("UdpEchoServerApplication", LOG_PREFIX_FUNC);

    std::string dataRate;
    uint64_t delay;

    CommandLine cmd;
    cmd.AddValue("datarate", "daterate", dataRate);
    cmd.AddValue("delay", "Link Delay", delay);
    cmd.Parse(argc, argv);


    NodeContainer c;
    c.Create(2);

    NodeContainer first = NodeContainer (c.Get (0), c.Get (1));

    PointToPointHelper apointToPoint;
    apointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    apointToPoint.SetChannelAttribute("Delay", TimeValue(MicroSeconds(delay)));

    NetDeviceContainer devices1;
    devices1 = apointToPoint.Install(first);

    InternetStackHelper stack1;
    stack1.Install(c);

    Ipv4AddressHelper address1, address2;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(devices1);
    // 1

    UdpEchoClientHelper echoClient1(interfaces1.GetAddress(1), 9);
    //echoClient1.SetAttribute("MaxPackets", UintegerValue(1500));
    //echoClient1.SetAttribute("Interval", TimeValue(MilliSeconds(1.0)));
    //echoClient1.SetAttribute("PacketSize", UintegerValue(1050));

    ApplicationContainer clientApps1;
    clientApps1.Add(echoClient1.Install(first.Get(0)));
    clientApps1.Start(Seconds(1.0)); // SetStartTime
    clientApps1.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer1(9);
    ApplicationContainer serverApps1(echoServer1.Install(first.Get(1)));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(11.0));

    apointToPoint.EnablePcapAll("ex5");

    Simulator::Run ();
    Simulator::Stop(Seconds(11.0));
    Simulator::Destroy ();
    return 0;
}
