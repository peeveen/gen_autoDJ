#include "pch.h"
#include <combaseapi.h>
#include <mfapi.h>
#include <math.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include "DJDefs.h"
#include "DJPrefs.h"

void Normalize(short* pWavData, int channels, DWORD cbSize) {
  int max = 0, min = 0;
  for (DWORD f = 0; f < cbSize; f += channels) {
    for (int g = 0; g < channels; ++g) {
      short val = pWavData[f + g];
      if (val >= 0 && val > max)
        max = val;
      if (val < 0 && val < min)
        min = val;
    }
  }
  double posMult = 32767.0 / max;
  double negMult = -32768.0 / min;
  for (DWORD f = 0; f < cbSize; f += channels) {
    double total = 0;
    for (int g = 0; g < channels; ++g) {
      short val = pWavData[f + g];
      pWavData[f + g] = (short)(val * (val >= 0 ? posMult : negMult));
    }
  }
}

void GetStartStopPositions(short* pWavData, int channels, UINT64 bytesPerSecond, DWORD cbSize, short startThreshold, short stopThreshold, StartStopPositions* pSSPos) {
  for (DWORD f = 0; f < cbSize; f += channels) {
    double total = 0;
    for (int g = 0; g < channels; ++g) {
      total += abs(pWavData[f + g]);
    }
    total /= channels;
    if (total > startThreshold) {
      pSSPos->startPos = (DWORD)(((f * channels) / (double)bytesPerSecond) * 1000.0);
      break;
    }
  }
  for (DWORD f = cbSize - channels; f > 0; f -= channels) {
    double total = 0;
    for (int g = 0; g < channels; ++g) {
      total += abs(pWavData[f + g]);
    }
    total /= channels;
    if (total > stopThreshold) {
      pSSPos->stopPos = (DWORD)(((f * channels) / (double)bytesPerSecond) * 1000.0);
      break;
    }
  }
}

bool GetStartStopPositions(const WCHAR *pszFilename, bool isKaraoke, StartStopPositions* pSSPos) {
  bool result = false;
  IMFSourceReader* pReader = NULL;
  HRESULT hr = ::MFCreateSourceReaderFromURL(pszFilename, NULL, &pReader);
  if (SUCCEEDED(hr)) {
    hr = pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (SUCCEEDED(hr)) {
      hr = pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
      if (SUCCEEDED(hr)) {
        IMFMediaType* pPartialType = NULL;
        hr = ::MFCreateMediaType(&pPartialType);
        if (SUCCEEDED(hr)) {
          hr = pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
          if (SUCCEEDED(hr)) {
            hr = pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            if (SUCCEEDED(hr)) {
              hr = pPartialType->SetUINT32(MF_MT_SAMPLE_SIZE, 4);
              if (SUCCEEDED(hr)) {
                hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pPartialType);
                if (SUCCEEDED(hr)) {
                  IMFMediaType* pUncompressedAudioType = NULL;
                  hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedAudioType);
                  if (SUCCEEDED(hr)) {
                    hr = pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
                    if (SUCCEEDED(hr)) {
                      PROPVARIANT prop;
                      hr = pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &prop);
                      if (SUCCEEDED(hr)) {
                        UINT32 cbBlockSize = ::MFGetAttributeUINT32(pUncompressedAudioType, MF_MT_AUDIO_BLOCK_ALIGNMENT, 0);
                        UINT32 channels = ::MFGetAttributeUINT32(pUncompressedAudioType, MF_MT_AUDIO_NUM_CHANNELS, 0);
                        UINT32 sampleSize = ::MFGetAttributeUINT32(pUncompressedAudioType, MF_MT_SAMPLE_SIZE, 0);
                        UINT64 cbBytesPerSecond = ::MFGetAttributeUINT32(pUncompressedAudioType, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0);
                        UINT64 songLength = prop.uhVal.QuadPart;
                        UINT64 cbAudioClipSize = (UINT64)(cbBytesPerSecond * (songLength / 10000000.0));
                        cbAudioClipSize = min(cbAudioClipSize, MAXDWORD);
                        cbAudioClipSize = ((cbAudioClipSize / cbBlockSize) + channels) * cbBlockSize;

                        DWORD dwFlags(0);
                        IMFSample* pSample = NULL;
                        DWORD cbWavData(0);
                        BYTE* pData = (BYTE*)malloc((size_t)cbAudioClipSize);
                        for (;;) {
                          hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &dwFlags, NULL, &pSample);
                          if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
                            break;
                          IMFMediaBuffer* pBuffer = NULL;
                          hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                          if (SUCCEEDED(hr)) {
                            DWORD cbBuffer = 0;
                            BYTE* pAudioData = NULL;
                            hr = pBuffer->Lock(&pAudioData, NULL, &cbBuffer);
                            if (SUCCEEDED(hr)) {
                              memcpy(pData + cbWavData, pAudioData, cbBuffer);
                              cbWavData += cbBuffer;
                              pBuffer->Unlock();
                            }
                            pBuffer->Release();
                          }
                          pSample->Release();
                        }
                        short* pWavData = (short*)pData;
                        cbWavData /= sizeof(short);
                        Normalize(pWavData, channels, cbWavData);
                        GetStartStopPositions(pWavData, channels, cbBytesPerSecond, cbWavData, g_nStartThreshold, isKaraoke?g_nKaraokeStopThreshold:g_nStopThreshold,pSSPos);
                        result = true;
                        free(pWavData);
                      }
                    }
                    pUncompressedAudioType->Release();
                  }
                }
              }
            }
          }
          pPartialType->Release();
        }
      }
    }
    pReader->Release();
  }
  return result;
}