/**
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */

#include "LinkContext.h"

#include "opendnp3/link/PriLinkLayerStates.h"
#include "opendnp3/link/SecLinkLayerStates.h"
#include "opendnp3/link/LinkFrame.h"
#include "opendnp3/link/ILinkRouter.h"

#include "opendnp3/ErrorCodes.h"


using namespace openpal;

namespace opendnp3
{

LinkContext::LinkContext(openpal::LogRoot& root, openpal::IExecutor& executor, opendnp3::ILinkListener& linkListener, ILinkSession& session, const LinkConfig& config_) :
	logger(root.GetLogger()),
	config(config_),
	pSegments(nullptr),
	txMode(LinkTransmitMode::Idle),
	numRetryRemaining(0),
	pExecutor(&executor),
	rspTimeoutTimer(executor),
	keepAliveTimer(executor),
	nextReadFCB(false),
	nextWriteFCB(false),
	isOnline(false),
	keepAliveTimeout(false),
	isRemoteReset(false),
	lastMessageTimestamp(executor.GetTime()),
	pRouter(nullptr),
	pPriState(&PLLS_Idle::Instance()),
	pSecState(&SLLS_NotReset::Instance()),
	pListener(&linkListener),
	pSession(&session)
{}

bool LinkContext::OnLowerLayerUp()
{
	if (this->isOnline)
	{
		SIMPLE_LOG_BLOCK(logger, flags::ERR, "Layer already online");
		return false;
	}	

	this->isOnline = true;

	// no reason to trigger a keep-alive until we've actually expired
	this->lastMessageTimestamp = this->pExecutor->GetTime();
	this->keepAliveTimer.Start(config.KeepAliveTimeout, [this]() { this->OnKeepAliveTimeout(); });

	this->PostStatusCallback(opendnp3::LinkStatus::UNRESET);

	if (pUpperLayer)
	{
		this->pUpperLayer->OnLowerLayerUp();
	}
	
	return true;
}

bool LinkContext::OnLowerLayerDown()
{
	if (!isOnline)
	{
		SIMPLE_LOG_BLOCK(logger, flags::ERR, "Layer is not online");
		return false;
	}

	isOnline = false;
	keepAliveTimeout = false;
	isRemoteReset = false;
	pSegments = nullptr;
	txMode = LinkTransmitMode::Idle;
	pendingPriTx.Clear();
	pendingSecTx.Clear();

	rspTimeoutTimer.Cancel();
	keepAliveTimer.Cancel();

	pPriState = &PLLS_Idle::Instance();
	pSecState = &SLLS_NotReset::Instance();

	this->PostStatusCallback(opendnp3::LinkStatus::UNRESET);

	if (pUpperLayer)
	{
		this->pUpperLayer->OnLowerLayerDown();
	}

	return true;
}

openpal::RSlice LinkContext::FormatPrimaryBufferWithConfirmed(const openpal::RSlice& tpdu, bool FCB)
{
	auto dest = this->priTxBuffer.GetWSlice();
	auto output = LinkFrame::FormatConfirmedUserData(dest, config.IsMaster, FCB, config.RemoteAddr, config.LocalAddr, tpdu, tpdu.Size(), &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, output, 10, 18);
	return output;
}

RSlice LinkContext::FormatPrimaryBufferWithUnconfirmed(const openpal::RSlice& tpdu)
{
	auto dest = this->priTxBuffer.GetWSlice();
	auto output = LinkFrame::FormatUnconfirmedUserData(dest, config.IsMaster, config.RemoteAddr, config.LocalAddr, tpdu, tpdu.Size(), &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, output, 10, 18);
	return output;
}

void LinkContext::QueueTransmit(const RSlice& buffer, bool primary)
{
	if (txMode == LinkTransmitMode::Idle)
	{
		txMode = primary ? LinkTransmitMode::Primary : LinkTransmitMode::Secondary;
		pRouter->BeginTransmit(buffer, pSession);
	}
	else
	{
		if (primary)
		{
			pendingPriTx.Set(buffer);
		}
		else
		{
			pendingSecTx.Set(buffer);
		}
	}
}

void LinkContext::QueueAck()
{
	auto dest = secTxBuffer.GetWSlice();
	auto buffer = LinkFrame::FormatAck(dest, config.IsMaster, false, config.RemoteAddr, config.LocalAddr, &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, buffer, 10, 18);
	this->QueueTransmit(buffer, false);
}

void LinkContext::QueueLinkStatus()
{
	auto dest = secTxBuffer.GetWSlice();
	auto buffer = LinkFrame::FormatLinkStatus(dest, config.IsMaster, false, config.RemoteAddr, config.LocalAddr, &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, buffer, 10, 18);
	this->QueueTransmit(buffer, false);
}

void LinkContext::QueueResetLinks()
{
	auto dest = priTxBuffer.GetWSlice();
	auto buffer = LinkFrame::FormatResetLinkStates(dest, config.IsMaster, config.RemoteAddr, config.LocalAddr, &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, buffer, 10, 18);
	this->QueueTransmit(buffer, true);
}

void LinkContext::QueueRequestLinkStatus()
{
	auto dest = priTxBuffer.GetWSlice();
	auto buffer = LinkFrame::FormatRequestLinkStatus(dest, config.IsMaster, config.RemoteAddr, config.LocalAddr, &logger);
	FORMAT_HEX_BLOCK(logger, flags::LINK_TX_HEX, buffer, 10, 18);
	this->QueueTransmit(buffer, true);
}

void LinkContext::ResetRetry()
{
	this->numRetryRemaining = config.NumRetry;
}

bool LinkContext::Retry()
{
	if (numRetryRemaining > 0)
	{
		--numRetryRemaining;
		return true;
	}
	else
	{
		return false;
	}
}

void LinkContext::PushDataUp(const openpal::RSlice& data)
{
	if (pUpperLayer)
	{
		pUpperLayer->OnReceive(data);
	}
}

void LinkContext::PostStatusCallback(opendnp3::LinkStatus status)
{
	auto callback = [this, status]()
	{
		this->pListener->OnStateChange(status);
	};

	pExecutor->PostLambda(callback);
}

void LinkContext::CompleteSendOperation(bool success)
{
	this->pSegments = nullptr;

	if (pUpperLayer)
	{
		auto callback = [this, success]()
		{
			this->pUpperLayer->OnSendResult(success);
		};

		pExecutor->PostLambda(callback);
	}
}

void LinkContext::TryStartTransmission()
{
	if (this->keepAliveTimeout)
	{
		this->pPriState = &pPriState->TrySendRequestLinkStatus(*this);
	}

	if (this->pSegments)
	{
		this->pPriState = (this->config.UseConfirms) ? &pPriState->TrySendConfirmed(*this, *pSegments) : &pPriState->TrySendUnconfirmed(*this, *pSegments);
	}
}

void LinkContext::OnKeepAliveTimeout()
{
	auto now = this->pExecutor->GetTime();

	auto elapsed = this->pExecutor->GetTime().milliseconds - this->lastMessageTimestamp.milliseconds;

	if (elapsed >= this->config.KeepAliveTimeout.GetMilliseconds())
	{
		this->lastMessageTimestamp = now;
		this->keepAliveTimeout = true;
		this->pListener->OnKeepAliveTimeout();
	}

	// No matter what, reschedule the timer based on last message timestamp
	MonotonicTimestamp expiration(this->lastMessageTimestamp.milliseconds + config.KeepAliveTimeout);
	this->keepAliveTimer.Start(expiration, [this]() { this->OnKeepAliveTimeout(); });
}

void LinkContext::OnResponseTimeout()
{
	this->pPriState = &(this->pPriState->OnTimeout(*this));

	this->TryStartTransmission();
}

void LinkContext::StartTimer()
{
	rspTimeoutTimer.Start(
		TimeDuration(config.Timeout),
		[this]()
	{
		this->OnResponseTimeout();
	}
	);
}

void LinkContext::CancelTimer()
{
	rspTimeoutTimer.Cancel();
}

void LinkContext::FailKeepAlive(bool timeout)
{
	this->keepAliveTimeout = false;

	if (timeout)
	{
		this->pListener->OnKeepAliveFailure();
	}
}

void LinkContext::CompleteKeepAlive()
{
	this->keepAliveTimeout = false;
}


}

