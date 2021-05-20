#include "streaming-helper.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/yans-wifi-phy.h"

//custom

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("assn3");

int
main (int argc, char *argv[])
{
    //LogComponentEnable("assn3", LOG_LEVEL_INFO);
    LogComponentEnable("StreamingClientApplication", LOG_LEVEL_INFO);

    uint32_t payloadSize = 1472;

    // 1. Create Nodes STA and AP
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // 2. Create PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create());
    phy.Set ("Antennas", UintegerValue (3));
    phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (3));
    phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (3));
    phy.Set ("RxNoiseFigure", DoubleValue (45.0));
    phy.Set ("ChannelWidth", UintegerValue (80));

    // 3. Create MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid ("assn3");
    mac.SetType ("ns3::StaWifiMac",
            "Ssid", SsidValue (ssid),
            "ActiveProbing", BooleanValue(false));

    // 4. Create WLAN setting
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("VhtMcs9"),
            "ControlMode", StringValue ("VhtMcs0"));

    // 5. Create NetDevices
    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiStaNode);

    mac.SetType ("ns3::ApWifiMac",
            "Ssid", SsidValue (ssid),
            "EnableBeaconJitter", BooleanValue(false));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // 6. Create Network layer
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNode);

    Ipv4AddressHelper address;

    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterface;
    StaInterface = address.Assign (staDevice);
    Ipv4InterfaceContainer ApInterface;
    ApInterface = address.Assign (apDevice);
    NS_LOG_INFO(StaInterface.GetAddress(1));

    // 7. Locate nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (1.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNode);

    // 8. Create Transport Layer (UDP)
    // application level
    uint16_t udp_port = 9;
    uint16_t udp_port2 = 10;
    NS_LOG_INFO(StaInterface.GetAddress(0));
    //NS_LOG_INFO(ApInterface.GetAddress(0));

    StreamingStreamerHelper echoStreamer(StaInterface.GetAddress(0), udp_port);
    echoStreamer.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoStreamer.SetAttribute("Interval", TimeValue(Seconds((double)(1.0/90))));
    echoStreamer.SetAttribute("PacketSize", UintegerValue(payloadSize));
    echoStreamer.SetAttribute("ReceivePort", UintegerValue(udp_port2));
    ApplicationContainer streamerApp = echoStreamer.Install(wifiApNode.Get(0));
    streamerApp.Start(Seconds(0.0));
    streamerApp.Stop(Seconds(10.0));


    StreamingClientHelper echoClient(udp_port);
    echoClient.SetAttribute("ConsumeInterval", TimeValue(Seconds((double)(1.0/60))));
    echoClient.SetAttribute("GenerateInterval", TimeValue(Seconds((double)(1.0/20))));
    echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    echoClient.SetAttribute("RemoteAddress", AddressValue(ApInterface.GetAddress(0)));
    echoClient.SetAttribute("RemotePort", UintegerValue(udp_port2));

    ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    //UdpEchoServerHelper myServer (9);
    //ApplicationContainer serverApp = myServer.Install (wifiStaNode.Get(0));
    //serverApp.Start(Seconds(0.0));
    //serverApp.Stop(Seconds(simulationTime));

    //UdpEchoClientHelper myClient (StaInterface.GetAddress(0), 9);
    //myClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    //myClient.SetAttribute("Interval", TimeValue(Time(1.0/9000)));
    //myClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    //ApplicationContainer clientApp = myClient.Install(wifiApNode.Get(0));


    // 9. Simulation Run and calc throughput
    Simulator::Stop(Seconds(10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    //uint32_t totalPacketsRecv = DynamicCast<UdpServer> (serverApp.Get(0))->GetReceived();
    //double throughput = totalPacketsRecv * payloadSize * 8 / (simulationTime * 1000000.0);
    //std::cout << "Throughput: " << throughput << " Mbps" << '\n';
    return 0;
}
