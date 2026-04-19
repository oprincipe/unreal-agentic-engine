#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"

class UEpicUnrealMCPBridge;

/**
 * Runnable class for the MCP server thread
 */
class FMCPServerRunnable : public FRunnable
{
public:
	FMCPServerRunnable(UEpicUnrealMCPBridge* InBridge, const TSharedPtr<FSocket>& InListenerSocket);
	virtual ~FMCPServerRunnable() override;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

protected:
	void HandleClientConnection(TSharedPtr<FSocket> ClientSocket);
	void ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message);

private:
	UEpicUnrealMCPBridge* Bridge;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ClientSocket;
	bool bRunning;
}; 