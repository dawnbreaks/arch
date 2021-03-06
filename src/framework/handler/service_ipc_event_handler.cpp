/*
 * ServiceInternalMessageHandler.cpp
 *
 *  Created on: 2011-2-12
 *      Author: qiyingwang
 */
#include "framework/process/manager_process.hpp"
#include "framework/handler/service_ipc_event_handler.hpp"
#include "framework/event/ipc_event_factory.hpp"
#include "framework/vcs/virtual_channel_helper.hpp"
#include "net/socket_unix_address.hpp"
#include "util/file_helper.hpp"
#include "logging/logger_macros.hpp"

using namespace arch::framework;
using namespace arch::channel;
using namespace arch::channel::fifo;
using namespace arch::util;
using arch::net::SocketUnixAddress;
using arch::exception::Exception;

void ServiceIPCEventHandler::SetServiceProcess(ServiceProcess* proc)
{
	m_service = proc;
	m_has_dispatcher = m_service->GetManagerProcess()->GetDispatcherCount() > 0;
}

void ServiceIPCEventHandler::ChannelConnected(ChannelHandlerContext& ctx,
        ChannelStateEvent& e)
{
	Channel* ch = ctx.GetChannel();
	Address* addr = const_cast<Address*>(ch->GetRemoteAddress());
	Address* localaddr = const_cast<Address*>(ch->GetLocalAddress());
	if (NULL != addr && InstanceOf<SocketUnixAddress>(addr).OK)
	{
		SocketUnixAddress* un = (SocketUnixAddress*) addr;
		SocketUnixAddress* un1 = (SocketUnixAddress*) localaddr;
		DEBUG_LOG(
		        "Local %s Connected %s", un1->GetPath().c_str(), un->GetPath().c_str());
		if (m_service->GetManagerProcess()->IsManagerCtrlUnixSocketServer(
		        un->GetPath()))
		{
			ch->DetachFD();
			ch->SetIOEventCallback(
			        VirtualChannelHelper::CtrlChannelIOEventCallback,
			        AE_READABLE, ch);
//			make_fd_blocking(ch->GetReadFD());
		}
	}
}

void ServiceIPCEventHandler::ChannelClosed(ChannelHandlerContext& ctx,
        ChannelStateEvent& e)
{
	Channel* ch = ctx.GetChannel();
	Address* addr = const_cast<Address*>(ch->GetRemoteAddress());
	Address* localaddr = const_cast<Address*>(ch->GetLocalAddress());
	if (NULL != addr && InstanceOf<SocketUnixAddress>(addr).OK)
	{
		SocketUnixAddress* un = (SocketUnixAddress*) addr;
		SocketUnixAddress* un1 = (SocketUnixAddress*) localaddr;
		DEBUG_LOG(
		        "Local %s Closed %s", un1->GetPath().c_str(), un->GetPath().c_str());
		if (m_service->GetManagerProcess()->IsManagerIPCUnixSocketServer(
		        un->GetPath()))
		{
			m_service->SetIPCUnixSocket(NULL);
		}
		else if (m_service->GetManagerProcess()->IsManagerCtrlUnixSocketServer(
		        un->GetPath()))
		{
			m_service->SetCtrlChannel(NULL);
		}
		else
		{
			GetServiceProcess()->AddClosedDispatcherIPCChannel(ch);
			VirtualChannelHelper::ClearTable(ch);
		}
	}
}

void ServiceIPCEventHandler::MessageReceived(ChannelHandlerContext& ctx,
        MessageEvent<IPCEvent>& e)
{
#define CHECK_SEVICE_HANDLER() do{  \
		if (NULL == m_handler)  \
			{                   \
			    if( GetServiceProcess()->GetServiceType() == DISPATCHER_SERVICE_PROCESS_TYPE) return; \
				WARN_LOG("ServiceHandler is NULL for handling IPC event."); \
				return;                                                     \
			}                                                               \
         }while(0)


	IPCEvent* ipc_event = e.GetMessage();
	IPCEventType type = ipc_event->GetType();
	switch (type)
	{
		case SOCKET_MSG:
		{
			CHECK_SEVICE_HANDLER();
			SocketMessageEvent* msg = (SocketMessageEvent*) ipc_event;
			uint32 reportSocketChannelID = msg->GetChannelID();
			if (m_has_dispatcher)
			{
				uint32 realID = msg->GetChannelID();
				Channel* src_channel = e.GetChannel();
				VirtualChannelHelper::GetVirtualSocketID(src_channel, realID,
				        reportSocketChannelID);
			}
			if (NULL != msg->GetContent())
			{
				if (NULL != msg->GetSocketInetAddress())
				{
					m_handler->OnSocketMessage(reportSocketChannelID,
					        *(msg->GetContent()),
					        *(msg->GetSocketInetAddress()));
				}
				else
				{
					m_handler->OnSocketMessage(reportSocketChannelID,
					        *(msg->GetContent()));
				}
			}
			break;
		}
		case IPC_MSG:
		{
			CHECK_SEVICE_HANDLER();
			IPCMessageEvent* msg = (IPCMessageEvent*) ipc_event;
			if (NULL != msg->GetContent())
			{
				m_handler->OnIPCMessage(msg->GetSrcType(), msg->GetSrcIndex(),
				        *(msg->GetContent()));
			}
			break;
		}
		case ADMIN:
		{
			CHECK_SEVICE_HANDLER();
			IPCMessageEvent* msg = (IPCMessageEvent*) ipc_event;
			if (NULL != msg->GetContent())
			{
				AdminCommand cmd;
				if (0
				        == AdminCommandHandler::DecodeCommand(cmd,
				                msg->GetContent()))
				{
					std::string result;
					Buffer tmp;
					if (AdminCommandHandler::HandleCommand(cmd, result) >= 0)
					{
						tmp.EnsureWritableBytes(result.size() + 2);
						tmp.Printf("%s\r\n", result.c_str());
					}
					else
					{
						tmp.Printf("Failed to handler command:%s\r\n",
						        cmd.name.c_str());
					}
					VirtualChannelHelper::WriteSocket(cmd.admin_channel_id,
					        tmp);
				}
			}
			break;
		}
		case IPC_CTRL:
		{
			IPCCtrlEvent* event = (IPCCtrlEvent*) ipc_event;
			switch (event->GetIPCCtrlType())
			{
				case SERV_PROC_STARTED:
				{
					CHECK_SEVICE_HANDLER();
					m_handler->OnServiceProcessStarted(event->GetSrcType(),
					        event->GetSrcIndex());
					break;
				}
				case SERV_PROC_STOPED:
				{
					CHECK_SEVICE_HANDLER();
					m_handler->OnServiceProcessStoped(event->GetSrcType(),
					        event->GetSrcIndex());
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}
		case SOCKET_CTRL:
		{
			CHECK_SEVICE_HANDLER();
			SocketCtrlEvent* event = (SocketCtrlEvent*) ipc_event;
			uint32 reportSocketChannelID = event->GetSocketChannelID();
			if (m_has_dispatcher)
			{
				uint32 realID = event->GetSocketChannelID();
				Channel* src_channel = ctx.GetChannel();
				VirtualChannelHelper::GetVirtualSocketID(src_channel, realID,
				        reportSocketChannelID);
			}
			switch (event->GetSocketCtrlType())
			{
				case SOCKET_OPENED:
				{
					m_handler->OnSocketOpened(reportSocketChannelID);
					break;
				}
				case SOCKET_CONNECTED:
				{
					if (NULL != event->GetAddress())
					{
						m_handler->OnSocketConnected(reportSocketChannelID,
						        *(event->GetAddress()));
					}
					else if (NULL != event->GetUnixAddress())
					{
						m_handler->OnSocketConnected(reportSocketChannelID,
						        *(event->GetUnixAddress()));
					}

					break;
				}
				case SOCKET_CLOSED:
				{
					m_handler->OnSocketClosed(reportSocketChannelID);
					if (m_has_dispatcher)
					{
						VirtualChannelHelper::ClearTable(ctx.GetChannel(),
						        event->GetSocketChannelID());
					}
					break;
				}
				case SOCKET_CONNECT_FAILED:
				{
					if (NULL != event->GetAddress())
					{
						m_handler->OnSocketConnectFailed(reportSocketChannelID,
						        *(event->GetAddress()));
					}
					else if (NULL != event->GetUnixAddress())
					{
						m_handler->OnSocketConnectFailed(reportSocketChannelID,
						        *(event->GetUnixAddress()));
					}
					if (m_has_dispatcher)
					{
						VirtualChannelHelper::ClearTable(ctx.GetChannel(),
						        event->GetSocketChannelID());
					}
					break;
				}
				default:
				{
					ERROR_LOG(
					        "Unknown socket ctrl type %u", event->GetSocketCtrlType());
					break;
				}
			}
			break;
		}
		default:
		{
			ERROR_LOG("Unknown ipc event type %u", ipc_event->GetType());
			break;
		}
	}
}
