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
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/seq-ts-header.h"
#include "streaming-streamer.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("StreamingStreamerApplication");

NS_OBJECT_ENSURE_REGISTERED (StreamingStreamer);

TypeId
StreamingStreamer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StreamingStreamer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<StreamingStreamer> ()
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&StreamingStreamer::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&StreamingStreamer::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&StreamingStreamer::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&StreamingStreamer::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&StreamingStreamer::SetDataSize,
                                         &StreamingStreamer::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&StreamingStreamer::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamingStreamer::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&StreamingStreamer::m_txTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamingStreamer::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("ReceivePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&StreamingStreamer::m_recvPort),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

StreamingStreamer::StreamingStreamer ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_resent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;
  seqNumber = 0;
  chkNumber = 0;
  lossNumber = 0;
  reNumber = 0;
  lastEchoNumber = 0;
  send_state = 1;
  r_socket = 0;
}

StreamingStreamer::~StreamingStreamer()
{
  NS_LOG_FUNCTION (this);
  //NS_LOG_INFO("Q1)Packet Drop Ratio:" << (float(lossNumber + seqNumber - lastEchoNumber)*100. / float(seqNumber)) << "%");
  //NS_LOG_INFO("Q2)Packet Retransmit Success Ratio:" << (float(reNumber) * 100. / float(lossNumber)) << "%");
  m_socket = 0;

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
}

void 
StreamingStreamer::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
StreamingStreamer::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

void
StreamingStreamer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
StreamingStreamer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (r_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      r_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_recvPort);
      //NS_LOG_INFO("StreamingClient::"+ std::to_string( local));
      if (r_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
    }
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      NS_LOG_INFO(m_peerAddress);
      if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
      else if (InetSocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else if (Inet6SocketAddress::IsMatchingType (m_peerAddress) == true)
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
          m_socket->Connect (m_peerAddress);
        }
      else
        {
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
        }
    }

  r_socket->SetRecvCallback (MakeCallback (&StreamingStreamer::HandleReadr, this));
  m_socket->SetRecvCallback (MakeCallback (&StreamingStreamer::HandleRead, this));
  m_socket->SetAllowBroadcast (true);
  ScheduleTransmit (Seconds (0.));
}

void 
StreamingStreamer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void 
StreamingStreamer::SetDataSize (uint32_t dataSize)
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
StreamingStreamer::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
StreamingStreamer::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
StreamingStreamer::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
StreamingStreamer::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
StreamingStreamer::ScheduleTransmit (Time dt)
{

  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &StreamingStreamer::Send, this);
}

void 
StreamingStreamer::Send (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_sendEvent.IsExpired ());
  if (send_state == 0) {
    if (m_sent < m_count)
    {
      ScheduleTransmit (m_interval);
    }
    return;
  }

  for (int i=0; i<100; i++)
  {
      Ptr<Packet> p;
      if (m_dataSize)
        {
          //
          // If m_dataSize is non-zero, we have a data buffer of the same size that we
          // are expected to copy and send.  This state of affairs is created if one of
          // the Fill functions is called.  In this case, m_size must have been set
          // to agree with m_dataSize
          //
          NS_ASSERT_MSG (m_dataSize == m_size, "StreamingStreamer::Send(): m_size and m_dataSize inconsistent");
          NS_ASSERT_MSG (m_data, "StreamingStreamer::Send(): m_dataSize but no m_data");
          p = Create<Packet> (m_data, m_dataSize);
        }
      else
        {
          //
          // If m_dataSize is zero, the client has indicated that it doesn't care
          // about the data itself either by specifying the data size by setting
          // the corresponding attribute or by not calling a SetFill function.  In
          // this case, we don't worry about it either.  But we do allow m_size
          // to have a value different from the (zero) m_dataSize.
          //
          p = Create<Packet> (m_size);
        }
      Address localAddress;
      m_socket->GetSockName (localAddress);
      // call to the trace sinks before the packet is actually sent,
      // so that tags added to the packet can be sent as well
      m_txTrace (p);
      if (Ipv4Address::IsMatchingType (m_peerAddress))
        {
          m_txTraceWithAddresses (p, localAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
        }
      else if (Ipv6Address::IsMatchingType (m_peerAddress))
        {
          m_txTraceWithAddresses (p, localAddress, Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
        }
      //NS_LOG_INFO(std::to_string(seqNumber));
      SeqTsHeader seqTs;
      seqTs.SetSeq(seqNumber++);
      p->AddHeader(seqTs);
      m_socket->Send (p);
      ++m_sent;
  }

  if (m_sent < m_count)
    {
      ScheduleTransmit (m_interval);
    }
}

void 
StreamingStreamer::ReTransmit (uint32_t pktNum)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p;
  if (m_dataSize)
    {
      NS_ASSERT_MSG (m_dataSize == m_size, "StreamingStreamer::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "StreamingStreamer::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      p = Create<Packet> (m_size);
    }
  Address localAddress;
  m_socket->GetSockName (localAddress);
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  SeqTsHeader seqTs;
  seqTs.SetSeq(pktNum);
  p->AddHeader(seqTs);
  m_socket->Send (p);
  ++m_resent;
  //NS_LOG_INFO("Packet Retrans:" << pktNum);
}
void
StreamingStreamer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
        socket->GetSockName (localAddress);
        SeqTsHeader seqTs;
        packet->RemoveHeader(seqTs);
        uint32_t seqNumber = seqTs.GetSeq();
        const uint32_t pause_type = -1;
        const uint32_t resume_type = -2;
        // todo
        /*
         * packet_type
         * 1 : pause
         * 2 : resume
         * etc : general echo packet
         *
         */
        switch(seqNumber) {
            case pause_type:
                send_state = 0;
                break;
            case resume_type:
                send_state = 1;
                break;
        }
    }
}
void
StreamingStreamer::HandleReadr (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
        socket->GetSockName (localAddress);
        SeqTsHeader seqTs;
        packet->RemoveHeader(seqTs);
        uint32_t seqNumber = seqTs.GetSeq();
        const uint32_t pause_type = -1;
        const uint32_t resume_type = -2;
        // todo
        /*
         * packet_type
         * 1 : pause
         * 2 : resume
         * etc : general echo packet
         *
         */
        switch(seqNumber) {
            case pause_type:
                send_state = 0;
                break;
            case resume_type:
                send_state = 1;
                break;
        }
    }
}

} // Namespace ns3
