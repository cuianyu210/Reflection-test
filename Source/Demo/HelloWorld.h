#pragma once

#include "CoreMinimal.h"
#include "HelloWorld.generated.h"

UCLASS()
class UHelloWorld : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FString Message;

    UPROPERTY(BlueprintReadOnly)
    int32 RepeatCount = 0;

    UPROPERTY()
    UObject* Child = nullptr;

    UFUNCTION(BlueprintCallable)
    void PrintHello();

    UFUNCTION(BlueprintCallable)
    void NativePing();
};
