/*
 *      Copyright (C) 2016 Team Kodi
 *      Copyright (C) 2016 Valve Corporation
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "AESinkSteamLink.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "utils/log.h"

// Steam Link audio API
#include "SLAudio.h"

#include <cstring>

#define SL_SAMPLE_RATE  48000 // TODO
#define SINK_FEED_MS    100 // TODO

using namespace STEAMLINK;

namespace
{
  void LogFunction(void *pContext, ESLAudioLog eLogLevel, const char *pszMessage)
  {
    // TODO: Problem that log messages end with a newline?
    switch (eLogLevel)
    {
    case k_ESLAudioLogDebug:
      CLog::Log(LOGDEBUG, "%s", pszMessage);
      break;
    case k_ESLAudioLogInfo:
      CLog::Log(LOGINFO, "%s", pszMessage);
      break;
    case k_ESLAudioLogWarning:
      CLog::Log(LOGWARNING, "%s", pszMessage);
      break;
    case k_ESLAudioLogError:
      CLog::Log(LOGERROR, "%s", pszMessage);
      break;
    default:
      break;
    }
  }
}

CAESinkSteamLink::CAESinkSteamLink() :
  m_context(nullptr),
  m_stream(nullptr)
{
  // TODO: Refcount to allow logging with multiple instances
  SLAudio_SetLogLevel(k_ESLAudioLogDebug);
  SLAudio_SetLogFunction(LogFunction, nullptr);
}

CAESinkSteamLink::~CAESinkSteamLink()
{
  Deinitialize();
  SLAudio_SetLogFunction(nullptr, nullptr);
}

bool CAESinkSteamLink::Initialize(AEAudioFormat &format, std::string &device)
{
  Deinitialize();

  format.m_dataFormat    = AE_FMT_S16NE;
  format.m_frames        = format.m_sampleRate * SINK_FEED_MS / 1000;
  format.m_frameSize     = format.m_channelLayout.Count() * (CAEUtil::DataFormatToBits(format.m_dataFormat) >> 3);
  m_format = format;

  CSLAudioContext* context = SLAudio_CreateContext();
  if (context)
  {
    CSLAudioStream* stream = SLAudio_CreateStream(context, m_format.m_sampleRate, m_format.m_channelLayout.Count(), format.m_frameSize);
    if (stream)
    {
      m_context = context;
      m_stream = stream;
    }
    else
    {
      CLog::Log(LOGERROR, "SteamLinkAudio: Failed to create stream");
      SLAudio_FreeContext(context);
    }
  }
  else
  {
    CLog::Log(LOGERROR, "SteamLinkAudio: Failed to create context");
  }

  return m_context != nullptr;
}

void CAESinkSteamLink::Deinitialize()
{
  if (m_stream)
  {
    SLAudio_FreeStream(static_cast<CSLAudioStream*>(m_stream));
    m_stream = nullptr;
  }
  if (m_context)
  {
    SLAudio_FreeContext(static_cast<CSLAudioContext*>(m_context));
    m_context = nullptr;
  }
}

double CAESinkSteamLink::GetCacheTotal()
{
  return SINK_FEED_MS / 1000.0; // TODO
}

unsigned int CAESinkSteamLink::AddPackets(uint8_t **data, unsigned int frames, unsigned int offset)
{
  unsigned int i;
  for (i = 0; i < frames; i++)
  {
    void* buffer = SLAudio_BeginFrame(static_cast<CSLAudioStream*>(m_stream));
    if (buffer)
    {
      std::memcpy(buffer, data[i] + offset, m_format.m_frameSize); // TODO: offset?
      SLAudio_SubmitFrame(static_cast<CSLAudioStream*>(m_stream));
    }
  }

  return i;
}

void CAESinkSteamLink::GetDelay(AEDelayStatus &status)
{
  uint32_t samples = SLAudio_GetQueuedAudioSamples(static_cast<CSLAudioStream*>(m_stream));

  unsigned int frames = samples / m_format.m_channelLayout.Count();
  if (samples % m_format.m_channelLayout.Count() != 0)
    frames++;

  double seconds = (double)frames / m_format.m_sampleRate;
  status.SetDelay(seconds);
}

void CAESinkSteamLink::Drain()
{
  // TODO
}

void CAESinkSteamLink::EnumerateDevicesEx(AEDeviceInfoList &deviceInfoList)
{
  CAEDeviceInfo info;

  info.m_deviceType = AE_DEVTYPE_PCM;
  info.m_deviceName = "SteamLink";
  info.m_displayName = "Low Latency Audio";
  info.m_displayNameExtra = "";
  info.m_channels += AE_CH_FL;
  info.m_channels += AE_CH_FR;
  info.m_sampleRates.push_back(SL_SAMPLE_RATE);
  info.m_dataFormats.push_back(AE_FMT_S16LE);
  info.m_wantsIECPassthrough = false; // TODO

  deviceInfoList.push_back(info);
}
