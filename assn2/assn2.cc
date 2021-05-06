#include "udp-reliable-helper.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

//custom

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("assn2");

int
main (int argc, char *argv[])
{
    LogComponentEnable("UdpReliableEchoClientApplication", LOG_LEVEL_INFO);

      Ptr<Node> nSrc1 = CreateObject<Node> ();
      Ptr<Node> nSrc2 = CreateObject<Node> ();
      Ptr<Node> nRtr = CreateObject<Node> ();
      Ptr<Node> nDst = CreateObject<Node> ();
      NodeContainer nodes = NodeContainer (nSrc1, nSrc2, nRtr, nDst);

      NodeContainer nSrc1nRtr = NodeContainer(nSrc1, nRtr);
      NodeContainer nSrc2nRtr = NodeContainer(nSrc2, nRtr);
      NodeContainer nRtrnDst  = NodeContainer(nRtr, nDst);

      InternetStackHelper stack;
      stack.Install (nodes);

      // Create P2P channels
      PointToPointHelper p2p;
      p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
      p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
      p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1500B"));

      NetDeviceContainer dSrc1dRtr = p2p.Install (nSrc1nRtr);
      NetDeviceContainer dSrc2dRtr = p2p.Install (nSrc2nRtr);
      NetDeviceContainer dRtrdDst  = p2p.Install (nRtrnDst);

      // Add IP addresses
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer iSrc1iRtr = ipv4.Assign (dSrc1dRtr);
      ipv4.SetBase ("10.1.2.0", "255.255.255.0");
      Ipv4InterfaceContainer iSrc2iRtr = ipv4.Assign (dSrc2dRtr);
      ipv4.SetBase ("10.1.3.0", "255.255.255.0");
      Ipv4InterfaceContainer iRtriDst = ipv4.Assign (dRtrdDst);

      // Set up the routing tables
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    uint16_t udp_port = 9;

    UdpReliableEchoClientHelper echoClient(iRtriDst.GetAddress(1), udp_port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app2;
    app2.Add(echoClient.Install(nSrc1));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(30.0));

    UdpReliableEchoServerHelper echoServer(udp_port);
    ApplicationContainer app3;
    app3.Add(echoServer.Install(nDst));
    app3.Start(Seconds(0.0));
    app3.Stop(Seconds(31.0));

    uint16_t port = 10;

    OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (iRtriDst.GetAddress(1), port)));
    onoff.SetAttribute("DataRate", DataRateValue(10000000));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime",	StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install (nSrc2);
    app.Start (Seconds (1.0));
    app.Stop (Seconds (30.0));

    PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
    app = sink.Install (nDst);
    app.Start (Seconds (0.0));
    app.Stop (Seconds (31.0));


//    p2p.EnablePcapAll("assn2");

    Simulator::Run ();
    Simulator::Stop(Seconds(33.0));
    Simulator::Destroy ();
    return 0;
}
