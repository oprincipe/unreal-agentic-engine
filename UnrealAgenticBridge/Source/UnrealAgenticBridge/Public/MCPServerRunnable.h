#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"

class UUnrealAgenticBridge;

/**
 * Runnable class for the MCP server thread
 */
class FMCPServerRunnable : public FRunnable {
public:
  FMCPServerRunnable(UUnrealAgenticBridge *InBridge,
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
  UUnrealAgenticBridge *Bridge;
  TSharedPtr<FSocket> ListenerSocket;
  TSharedPtr<FSocket> ClientSocket;
  bool bRunning;
};