#include <iostream>

#include "sip2/util/compat.hxx"
#include "sip2/util/Logger.hxx"
#include "sip2/util/Socket.hxx"

#include "sip2/sipstack/Transport.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/TransportMessage.hxx"

using namespace Vocal2;
using namespace std;

#define VOCAL_SUBSYSTEM Subsystem::TRANSPORT

const Data Transport::transportNames[Transport::MAX_TRANSPORT] =
 {
    Data("Unknown"),
    Data("UDP"),
    Data("TCP"),
    Data("TLS"),
    Data("SCTP"),
    Data("DCCP")
 };

Transport::Exception::Exception(const Data& msg, const Data& file, const int line) :
   BaseException(msg,file,line)
{
}

Transport::Transport(const Data& sendhost, int portNum, const Data& nic, Fifo<Message>& rxFifo) :
   mHost(sendhost),
   mPort(portNum), 
   mInterface(nic),
   mStateMachineFifo(rxFifo),
   mShutdown(false)
{
}

Transport::~Transport()
{
}


void
Transport::run()
{
   while (!mShutdown)
   {
      FdSet fdset; 
      fdset.reset();
      fdset.setRead(mFd);
      fdset.setWrite(mFd);
      int  err = fdset.selectMilliSeconds(0);
      if (err == 0)
      {
         try
         {
            assert(0);
            //process();
         }
         catch (BaseException& e)
         {
            InfoLog (<< "Uncaught exception: " << e);
         }
      }
      else
      {
         assert(0);
      }
   }
}

void
Transport::shutdown()
{
   mShutdown = true;
}

void
Transport::fail(const Data& tid)
{
   mStateMachineFifo.add(new TransportMessage(tid, true));
}

void
Transport::ok(const Data& tid)
{
   mStateMachineFifo.add(new TransportMessage(tid, false));
}


bool 
Transport::hasDataToSend() const
{
   return mTxFifo.messageAvailable();
}


void 
Transport::send( const Tuple& dest, const Data& d, const Data& tid)
{
   SendData* data = new SendData(dest, d, tid);
   assert(dest.port != -1);
   DebugLog (<< "Adding message to tx buffer to: " << dest << " " << d.escaped());
   mTxFifo.add(data); // !jf!
}

const Data&
Transport::toData(Transport::Type type)
{
   assert(type >= Transport::Unknown &&
          type < Transport::MAX_TRANSPORT);
   return Transport::transportNames[type];
}

Transport::Type
Transport::toTransport(const Data& type)
{
   for (Transport::Type i = Transport::Unknown; i < Transport::MAX_TRANSPORT; 
        i = static_cast<Transport::Type>(i + 1))
   {
      if (isEqualNoCase(type, Transport::transportNames[i]))
      {
         return i;
      }
   }
   assert(0);
   return Transport::Unknown;
};

void
Transport::stampReceived(SipMessage* message)
{
   //DebugLog (<< "adding new SipMessage to state machine's Fifo: " << message->brief());
   // set the received= and rport= parameters in the message if necessary !jf!
   if (message->isRequest() && !message->header(h_Vias).empty())
   {
      const Tuple& tuple = message->getSource();
      
#ifndef WIN32
      char received[255];
      inet_ntop(AF_INET, &tuple.ipv4.s_addr, received, sizeof(received));
      message->header(h_Vias).front().param(p_received) = received;
#else
      char * buf = inet_ntoa(tuple.ipv4); // !jf! not threadsafe
      message->header(h_Vias).front().param(p_received) = buf;
#endif

      if (message->header(h_Vias).front().exists(p_rport))
      {
         message->header(h_Vias).front().param(p_rport).port() = tuple.port;
      }
   }
}


Transport::Tuple::Tuple() : 
   port(0), 
   transportType(Unknown), 
   transport(0),
   connection(0)
{
   memset(&ipv4, 0, sizeof(ipv4));
}

Transport::Tuple::Tuple(in_addr pipv4,
                        int pport,
                        Transport::Type ptype)
   : ipv4(pipv4),
     port(pport),
     transportType(ptype),
     transport(0),
     connection(0)
{
}

bool Transport::Tuple::operator==(const Transport::Tuple& rhs) const
{
   return (memcmp(&ipv4, &rhs.ipv4, sizeof(ipv4)) == 0 &&
           port == rhs.port &&
           transportType == rhs.transportType && 
           connection == rhs.connection);
}

bool Transport::Tuple::operator<(const Transport::Tuple& rhs) const
{
   int c = memcmp(&ipv4, &rhs.ipv4, sizeof(ipv4));
   if (c < 0)
   {
      return true;
   }
   
   if (c > 0)
   {
      return false;
   }
   
   if (port < rhs.port)
   {
      return true;
   }
   
   if (port > rhs.port)
   {
      return false;
   }
   
   return transportType < rhs.transportType;
}

std::ostream&
Vocal2::operator<<(ostream& ostrm, const Transport::Tuple& tuple)
{
	ostrm << "[ " ;

#if defined(WIN32) 
//	ostrm   << inet_ntoa(tuple.ipv4);
	
#else	
	char str[128];
	ostrm << inet_ntop(AF_INET, &tuple.ipv4.s_addr, str, sizeof(str));
#endif	
	
	ostrm  << " , " 
	       << tuple.port
	       << " , "
	       << Transport::toData(tuple.transportType) 
	       << " , "
	       << tuple.transport 
	       << " ]";
	
	return ostrm;
}


#if ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) )

size_t 
__gnu_cxx::hash<Vocal2::Transport::Tuple>::operator()(const Vocal2::Transport::Tuple& tuple) const
{
   // assumes POD
   unsigned long __h = 0; 
   const char* start = (const char*)&tuple;
   const char* end = start + sizeof(tuple);
   for ( ; start != end; ++start)
   {
      __h = 5*__h + *start; // .dlb. weird hash
   }
   return size_t(__h);

}

#elif  defined(__INTEL_COMPILER )
size_t 
std::hash_value(const Vocal2::Transport::Tuple& tuple) 
{
   // assumes POD
   unsigned long __h = 0; 
   const char* start = (const char*)&tuple;
   const char* end = start + sizeof(tuple);
   for ( ; start != end; ++start)
   {
      __h = 5*__h + *start; // .dlb. weird hash
   }
   return size_t(__h);
}


#endif

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */
