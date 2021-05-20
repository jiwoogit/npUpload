/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/seq-ts-header.h"
#include <map>
#include <stdlib.h>

#include "streaming-client.h"

namespace ns3 {

uint32_t curpkt = -1;

NS_LOG_COMPONENT_DEFINE ("StreamingClientApplication");

NS_OBJECT_ENSURE_REGISTERED (StreamingClient);

TypeId
StreamingClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StreamingClient")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<StreamingClient> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&StreamingClient::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamingClient::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamingClient::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("ConsumeInterval",
                   "Interval between consuming frames",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&StreamingClient::interval_consumer),
                   MakeTimeChecker ())
    .AddAttribute ("GenerateInterval",
                   "Interval between consuming frames",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&StreamingClient::interval_generator),
                   MakeTimeChecker ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&StreamingClient::SetDataSize,
                                         &StreamingClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&StreamingClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&StreamingClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

StreamingClient::StreamingClient ()
{
  NS_LOG_FUNCTION (this);
  curFrame = 0;
  packet_buffer.clear();
  frame_buffer.clear();
  m_data = 0;
  m_dataSize = 0;
  m_consumeEvent = EventId();
  m_generateEvent = EventId();
}

StreamingClient::~StreamingClient()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  r_socket = 0;
}

void 
StreamingClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
StreamingClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void
StreamingClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
StreamingClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      //NS_LOG_INFO("StreamingClient::"+ std::to_string( local));
      if (m_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }

      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }
  if (r_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      r_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          if (r_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          r_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          if (r_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          r_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (r_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          r_socket->Connect (m_peerAddress);
        }
      else if (Inet6SocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (r_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          r_socket->Connect (m_peerAddress);
        }
      else
        {
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
        }
    }

  //r_socket->SetRecvCallback (MakeCallback (&StreamingStreamer::HandleRead, this));
  r_socket->SetAllowBroadcast (true);

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      if (m_socket6->Bind (local6) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      if (addressUtils::IsMulticast (local6))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket6);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, local6);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&StreamingClient::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&StreamingClient::HandleRead, this));
  ScheduleGenerator (Seconds (0.));
  ScheduleConsumer (Seconds (0.));
}

void 
StreamingClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_socket6 != 0) 
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  Simulator::Cancel (m_consumeEvent);
  Simulator::Cancel (m_generateEvent);
}

void 
StreamingClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
      double val = (double)std::rand() / RAND_MAX;
      if (val < 0.0)
      {
          continue;
      }
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      //if (InetSocketAddress::IsMatchingType (from))
      //  {
      //    NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
      //                 InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
      //                 InetSocketAddress::ConvertFrom (from).GetPort ());
      //  }
      //else if (Inet6SocketAddress::IsMatchingType (from))
      //  {
      //    NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
      //                 Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
      //                 Inet6SocketAddress::ConvertFrom (from).GetPort ());
      //  }

      SeqTsHeader seqTs;
      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();
      packet->RemoveHeader(seqTs);
      uint32_t currentSequenceNumber = seqTs.GetSeq();
      //NS_LOG_INFO(currentSequenceNumber);
      uint32_t frame_index = currentSequenceNumber / 100;
      uint32_t seq_pkt_number = currentSequenceNumber - 100 * frame_index;
      if ( packet_buffer.find(frame_index) == packet_buffer.end() ) {
          std::map <uint32_t, Ptr<Packet>> frame_tmp;
          packet_buffer.insert({frame_index, frame_tmp});
        }
      packet_buffer.at(frame_index).insert({seq_pkt_number, packet});
      NS_LOG_LOGIC ("Echoing packet");
      //m_from = from;
      //r_socket = socket;
      //socket->SendTo (packet, 0, from);
    }
}

void 
StreamingClient::ScheduleConsumer (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_consumeEvent = Simulator::Schedule (dt, &StreamingClient::Consume, this);
}

void
StreamingClient::Consume (void)
{
    if (frame_buffer.find(curFrame) == frame_buffer.end())
    {
        NS_LOG_INFO("FrameConsumerLog::NoConsume");
    }
    else
    {
        NS_LOG_INFO("FrameConsumerLog::Consume");
        frame_buffer.erase(curFrame);
    }
    uint32_t remain_frame = (uint32_t)frame_buffer.size();
    NS_LOG_INFO("FrameConsumerLog::RemainFrames: " + std::to_string(remain_frame));
    SeqTsHeader seqTs;
    Ptr<Packet> packet = Create<Packet> (m_size);
    packet->RemoveAllPacketTags ();
    packet->RemoveAllByteTags ();
    packet->RemoveHeader(seqTs);
    uint32_t seqNum;
    if (remain_frame > 30)
    {
        seqNum = -1;
        seqTs.SetSeq(seqNum);
        packet->AddHeader(seqTs);
        //r_socket->SendTo (packet, 0, m_peerAddress);
        r_socket->Send (packet);
    }
    else if (remain_frame < 5)
    {
        seqNum = -2;
        seqTs.SetSeq(seqNum);
        packet->AddHeader(seqTs);
        //r_socket->SendTo (packet, 0, m_peerAddress);
        r_socket->Send (packet);
    }
    curFrame++;

    ScheduleConsumer (interval_consumer);
    return;
}
void 
StreamingClient::ScheduleGenerator (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_consumeEvent = Simulator::Schedule (dt, &StreamingClient::Generate, this);
}

void
StreamingClient::Generate (void)
{
    uint8_t delete_flag;
    for (auto iter = packet_buffer.cbegin(); iter != packet_buffer.cend();)
    {
        delete_flag = 0;
        uint32_t frame_index = iter->first;
        std::map <uint32_t, Ptr<Packet>> frame_tmp;
        frame_tmp = iter->second;
        if (frame_tmp.size() >= 100)
        {
            if (frame_buffer.size() >= 40 || frame_index < curFrame)
            {
                frame_tmp.clear();
                delete_flag = 1;
            }
            else
            {
                frame_buffer.insert({frame_index, frame_index});
                frame_tmp.clear();
                delete_flag = 1;
            }
        }
        if (delete_flag == 1)
        {
            packet_buffer.erase(iter++);
        }
        else {
            ++iter;
        }
    }
    ScheduleGenerator (interval_generator);
    return;
}

} // Namespace ns3
