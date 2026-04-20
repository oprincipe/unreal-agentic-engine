#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"

class UUnrealAgenticEngine;

/**
 * Runnable class for the MCP server thread
 */
class FMCPServerRunnable : public FRunnable {
public:
  FMCPServerRunnable(UUnrealAgenticEngine *InBridge,
                     const TSharedPtr<FSocket> &InListenerSocket);
  virtual ~FMCPServerRunnable() override;

  // FRunnable interface
  virtual bool Init() override;
  virtual uint32 Run() override;
  virtual void Stop() override;
  virtual void Exit() override;

protected:
  void HandleClientConnection(const TSharedPtr<FSocket> &ClientSocket);
  void ProcessMessage(const TSharedPtr<FSocket> &Client,
                      const FString &Message);

private:
  UUnrealAgenticEngine *Bridge;
  TSharedPtr<FSocket> ListenerSocket;
  TSharedPtr<FSocket> ClientSocket;
  bool bRunning;
};