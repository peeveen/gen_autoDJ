#include "pch.h"
#include <combaseapi.h>
#include <mfapi.h>
#include <sys/types.h>
#include <math.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <mferror.h>
#include <wchar.h>
#include "DJDefs.h"
#include "DJPrefs.h"
#include "DJWindows.h"

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

void GetStartStopPositions(bool start,bool stop,short* pWavData, int channels, UINT64 bytesPerSecond, DWORD cbSize, short startThreshold, short stopThreshold, ULONG stopLimit, StartStopPositions* pSSPos) {
  if(start)
    for (DWORD f = 0; f < cbSize; f += channels) {
      double total = 0;
      for (int g = 0; g < channels; ++g) {
        total += abs(pWavData[f + g]);
      }
      total /= channels;
      if (total > startThreshold) {
        pSSPos->startPos = (DWORD)((((double)f * channels) / (double)bytesPerSecond) * 1000.0);
        break;
      }
    }
  if (stop) {
    DWORD songEnd = (DWORD)(((((double)cbSize - channels) * channels) / (double)bytesPerSecond) * 1000.0);
    DWORD upperLimit = (DWORD)((stopLimit / 1000.0) * bytesPerSecond);
    pSSPos->stopPos = stopLimit > songEnd ? songEnd : stopLimit;
    for (DWORD f = cbSize - channels; f > upperLimit; f -= channels) {
      double total = 0;
      for (int g = 0; g < channels; ++g) {
        total += abs(pWavData[f + g]);
      }
      total /= channels;
      if (total > stopThreshold) {
        pSSPos->stopPos = (DWORD)((((double)f * channels) / (double)bytesPerSecond) * 1000.0);
        break;
      }
    }
  }
}

struct GetStartStopPositionsParams {
  WCHAR szFilename[MAX_PATH+1];
  StartStopPositions* pSSPos;
  bool start;
  bool stop;
};

DWORD WINAPI GetStartStopPositionsThread(LPVOID pParam) {
  GetStartStopPositionsParams *pParams= (GetStartStopPositionsParams*)pParam;
  DWORD result = 0;
  IMFSourceReader* pReader = NULL;

  bool isKaraoke = false;
  WCHAR szCDGTest[MAX_PATH + 1] = { '0' };
  wcscpy_s(szCDGTest, pParams->szFilename);
  WCHAR* pszLastDot = wcsrchr(szCDGTest, '.');
  ULONG karaokeLimit = 0;
  if (pszLastDot) {
    wcscpy_s(pszLastDot, (MAX_PATH - (pszLastDot - szCDGTest)), L".cdg");
    struct _stat64i32 stat;
    if (!_wstat(szCDGTest, &stat)) {
      int instructions = stat.st_size / sizeof(CDGPacket);
      int fileSize = instructions * sizeof(CDGPacket);
      CDGPacket* pCDGFileContents = (CDGPacket*)malloc(fileSize);
      if (pCDGFileContents) {
        FILE* pCDGFile;
        errno_t error = _wfopen_s(&pCDGFile, szCDGTest, L"rb");
        if (!error && pCDGFile) {
          int read = fread(pCDGFileContents, sizeof(CDGPacket), instructions, pCDGFile);
          if (read == instructions) {
            for (int f = instructions - 1; f >= 0; --f) {
              BYTE cmd = pCDGFileContents[f].command & 0x3F;
              BYTE instr = pCDGFileContents[f].instruction & 0x3F;
              // CDG_INSTR_TILE_BLOCK_XOR
              if (cmd == 0x09 && instr == 38) {
                karaokeLimit = (ULONG)((f / 300.0) * 1000.0);
                break;
              }
            }
          }
          fclose(pCDGFile);
        }
        free(pCDGFileContents);
      }
    }
  }

  HRESULT hr = ::MFCreateSourceReaderFromURL(pParams->szFilename, NULL, &pReader);
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
                        double songLengthSeconds = (songLength / 10000000.0);
                        // Allocate an extra second just to be on the safe side. Sometimes it overspills.
                        UINT64 cbAudioClipSize = (UINT64)(cbBytesPerSecond * (songLengthSeconds+1));
                        cbAudioClipSize = min(cbAudioClipSize, MAXDWORD);
                        cbAudioClipSize = ((cbAudioClipSize / cbBlockSize) + channels) * cbBlockSize;

//                        WCHAR sz[100];
//                        int nSeconds = (int)songLengthSeconds;
//                        wsprintf(sz, L"sampleSize=%d\nchannels=%d\nsongLength=%d\nbytesPerSecond=%d\nbufferSize=%d",sampleSize,channels,nSeconds, (long)cbBytesPerSecond,(long)cbAudioClipSize);
//                        ::MessageBox(NULL, sz, L"", MB_OK);

                        DWORD dwFlags(0);
                        IMFSample* pSample = NULL;
                        DWORD cbWavData(0);
                        BYTE* pData = (BYTE*)malloc((size_t)cbAudioClipSize);
                        if (pData) {
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
                                long remainingSpace = (long)cbAudioClipSize - cbWavData;
                                if (remainingSpace > 0) {
                                  long amountToCopy = min(remainingSpace, (long)cbBuffer);
                                  memcpy(pData + cbWavData, pAudioData, amountToCopy);
                                  cbWavData += amountToCopy;
                                }
                                pBuffer->Unlock();
                              }
                              pBuffer->Release();
                            }
                            pSample->Release();
                          }
                          short* pWavData = (short*)pData;
                          cbWavData /= sizeof(short);

                          Normalize(pWavData, channels, cbWavData);
                          GetStartStopPositions(pParams->start,pParams->stop,pWavData, channels, cbBytesPerSecond, cbWavData, g_nStartThreshold, isKaraoke ? g_nKaraokeStopThreshold : g_nStopThreshold, karaokeLimit, pParams->pSSPos);
                          ::InvalidateRect(g_hDJWindow, NULL, FALSE);
                          result = 1;
                          free(pWavData);
                        }
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
  free(pParam);
  return result;
}

void GetStartStopPositions(bool start,bool stop,const WCHAR* pszFilename, StartStopPositions* pSSPos) {
  GetStartStopPositionsParams* pParams = (GetStartStopPositionsParams *)malloc(sizeof(GetStartStopPositionsParams));
  if (pParams) {
    pParams->start = start;
    pParams->stop = stop;
    wcscpy_s(pParams->szFilename, pszFilename);
    pParams->pSSPos = pSSPos;
    ::CreateThread(NULL, 0, GetStartStopPositionsThread, pParams, 0, NULL);
  }
}
