/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 *
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "new-send-application.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

#include <cstdio>
#include <cstdlib>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NewSendApplication");

NS_OBJECT_ENSURE_REGISTERED (NewSendApplication);

TypeId
NewSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NewSendApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<NewSendApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&NewSendApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&NewSendApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("Local", "The address of the source",
                   AddressValue (),
                   MakeAddressAccessor (&NewSendApplication::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&NewSendApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RecvBytes",
                   "The total number of bytes to receive. ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&NewSendApplication::resp_size),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BrowserNum",
                   "The browser Number ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&NewSendApplication::browserNum),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&NewSendApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&NewSendApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


NewSendApplication::NewSendApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0)
{
  NS_LOG_FUNCTION (this);
  response_bytes = 0;
  request_complete = false;
  //port = InetSocketAddress::ConvertFrom (m_local).GetPort ();
  // port = 2000;
}

NewSendApplication::~NewSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
NewSendApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

void NewSendApplication::SetConsecPageCounter(uint32_t *ctr)
{
  consecPageCounter = ctr;
}

void NewSendApplication::SetSecondaryRequestCounter(uint32_t *ctr)
{
  secondaryRequestCounter = ctr;
}

void NewSendApplication::SetFunction(void (*func) (request_param_t))
{
  param.func = func;
}

Ptr<Socket>
NewSendApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
NewSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void NewSendApplication::StartApplication (void) // Called at time specified by Start
{
  param.browserNum = browserNum;
  param.consecPageCounter = consecPageCounter;
  param.destServer = InetSocketAddress(InetSocketAddress::ConvertFrom(m_peer).GetIpv4 (), InetSocketAddress::ConvertFrom (m_peer).GetPort ());
  param.secondaryRequestCounter = secondaryRequestCounter;
  NS_LOG_FUNCTION (this);
  // printf("starting APP\n");
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      port = InetSocketAddress::ConvertFrom (m_local).GetPort ();

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using NewSend with an incompatible socket type. "
                          "NewSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }
      // printf("before bind\n");
      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind (m_local); //m_local
        }
      
      // printf("Created sink at %d", port+1);
      // printf("before connect\n");
      m_socket->Connect (m_peer);
      param.start = Simulator::Now();
      // std::cout<<"My start time is "<<Simulator::Now().GetSeconds()<<"\n";
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                           InetSocketAddress (Ipv4Address::GetAny (), port+1), param, resp_size);
      ApplicationContainer sinkApps = sink.Install (GetNode());
      // sinkApps.Start(Simulator::Now());
      // sinkptr = DynamicCast<PacketSink> (sinkApps.Get(0));

      // printf("after connect\n" );
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&NewSendApplication::ConnectionSucceeded, this),
        MakeCallback (&NewSendApplication::ConnectionFailed, this));
      // m_socket->SetSendCallback (
      //   MakeCallback (&NewSendApplication::DataSend, this));
      // m_socket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
        // MakeCallback (&NewPacketSink::HandleAccept, this));
      // m_socket->SetRecvCallback (MakeCallback (&NewSendApplication::HandleRead, this));
      //Need to add receive callback here. Increments the response_bytes counter. Once we receive resp_size amount of data, the application will send the secondary reqs
    }
  // printf("connection status: %d",(int) m_connected);
  // if (m_connected)
  //   {
  //     SendData ();
  //   }
}

void NewSendApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("NewSendApplication found null socket to close in StopApplication");
    }
}


// Private helpers
void create_packet_payload(uint32_t input, uint8_t opcode, uint8_t* buffer, uint32_t buffer_size)
{
  buffer[0] = opcode;
  for(int i = 4; i >= 1; i--)
  {
      buffer[i] = input%256;
      input /= 256;
  } 
  for (uint32_t i = 5; i < buffer_size; i++)
  {
    buffer[i] = 0;
  }   
}
void NewSendApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);
  //Remove this later
  //m_maxBytes = 5000;
  //resp_size = 8000;


  uint32_t request_size = m_maxBytes;
  uint32_t toSend = m_sendSize;
  uint8_t *buffer = new uint8_t[toSend];
  // printf("To send size: %d\n", toSend);
  // Opcodes 
  // 0: extra data. don't read the rest of the packet. 
  // 1: primary request

  uint8_t opcode = 1;  
  if(request_size < 5)
  {
    request_size = 5;
  }
  if(request_size < toSend)
  {
    // printf("request size %d\n", request_size);
    toSend = request_size;
  }
  if (resp_size == 0)
    resp_size++;
  // printf("in senddata\n");
  // Create sink at current port + 1


  // First packet contains opcode 1,resp_size (total 5 bytes)
  // printf("SendApp: Sending request for %d bytes with opcode 1\n", (int) resp_size);
  create_packet_payload(resp_size, opcode, buffer, toSend);
  Ptr<Packet> packet = Create<Packet> (buffer, toSend);
  int actual = m_socket->Send (packet);
  if (actual > 0)
  {
    m_totBytes += actual;
  }       
  delete [] buffer; 
  

  while (m_totBytes < request_size)
    { // Time to send more
      
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (m_sendSize, request_size - m_totBytes);
        }
      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      m_txTrace (packet);     
      int actual = m_socket->Send (packet);
      // printf("SendApp: Sending 1 more packet. %d bytes sent so far \n", (int) m_totBytes);
      if (actual > 0)
        {
          m_totBytes += actual;
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed ip.
      if ((unsigned)actual != toSend)
        {
          break;
        }
    }
    request_complete = true;
    
  // Loop till we receive the full primary response. The response_bytes counter is incremented by the recv callback
  //  while(sinkptr->GetTotalRx() < resp_size) 
  //  {
  //    printf("SendApp: Response received so far: %d", sinkptr->GetTotalRx());
  //  }



  // Check if time to close (all sent)
  if (m_connected)
    {
      m_socket->Close ();
      m_connected = false;
    }
    // StopApplication();
}

bool NewSendApplication::ResponseComplete()
{
    // if(sinkptr->GetTotalRx() < resp_size)
    // {
    //   // printf("SendApp: Response received so far: %d", sinkptr->GetTotalRx());
    //   return false;
    // }
    // else
       return true;
} 


Address NewSendApplication::GetDestinationAddress()
{
  return m_peer;
}


// void NewSendApplication::HandleRead (Ptr<Socket> socket)
// {
//   NS_LOG_FUNCTION (this << socket);
//   Ptr<Packet> packet;
//   Address from;
//   while ((packet = socket->RecvFrom (from)))
//     {
//       if (packet->GetSize () == 0)
//         { //EOF
//           break;
//         }
//       response_bytes += packet->GetSize ();
//       printf("SendApp: Received response %u bytes so far\n", response_bytes);
//     }
// }

void NewSendApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  // printf("ConnectionSucceeded\n");
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("NewSendApplication Connection succeeded");
  m_connected = true;
  SendData ();
 
}

void NewSendApplication::ConnectionFailed (Ptr<Socket> socket)
{
  // printf("ConnectionFailed\n");
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("NewSendApplication, Connection Failed");
}

// void NewSendApplication::DataSend (Ptr<Socket>, uint32_t)
// {
//   NS_LOG_FUNCTION (this);

//   if (m_connected)
//     { // Only send new data if the connection has completed
//       SendData ();
//     }
// }



} // Namespace ns3
