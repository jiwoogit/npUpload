#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ex4");

int
main (int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_PREFIX_TIME);
    LogComponentEnable("UdpEchoServerApplication", LOG_PREFIX_TIME);
    LogComponentEnable("UdpEchoClientApplication", LOG_PREFIX_FUNC);
    LogComponentEnable("UdpEchoServerApplication", LOG_PREFIX_FUNC);

    NodeContainer c;
    c.Create(3);

    NodeContainer first = NodeContainer (c.Get (0), c.Get (1));
    NodeContainer second = NodeContainer (c.Get (1), c.Get (2));

    PointToPointHelper apointToPoint;
    apointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    apointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper bpointToPoint;
    bpointToPoint.SetDeviceAttribute("DataRate", StringValue("0.1Mbps"));
    bpointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices1;
    devices1 = apointToPoint.Install(first);
    NetDeviceContainer devices2;
    devices2 = bpointToPoint.Install(second);

    InternetStackHelper stack1;
    stack1.Install(c);

    Ipv4AddressHelper address1, address2;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address1.Assign(devices1);
    Ipv4InterfaceContainer interfaces2 = address2.Assign(devices2);
    
    // 1

    UdpEchoClientHelper echoClient1(interfaces1.GetAddress(1), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1500));
    echoClient1.SetAttribute("Interval", TimeValue(MilliSeconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1050));

    ApplicationContainer clientApps1;
    clientApps1.Add(echoClient1.Install(first.Get(0)));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(4.0));

    UdpEchoServerHelper echoServer1(9);
    ApplicationContainer serverApps1(echoServer1.Install(first.Get(1)));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(5.0));

    // 2

    UdpEchoClientHelper echoClient2(interfaces2.GetAddress(1), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1500));
    echoClient2.SetAttribute("Interval", TimeValue(MilliSeconds(10.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1050));

    ApplicationContainer clientApps2;
    clientApps2.Add(echoClient2.Install(second.Get(0)));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(4.0));

    UdpEchoServerHelper echoServer2(9);
    ApplicationContainer serverApps2(echoServer2.Install(second.Get(1)));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(5.0));

    Simulator::Run ();
    Simulator::Stop(Seconds(11.0));
    Simulator::Destroy ();
    return 0;
}
