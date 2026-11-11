#pragma once

// 这些宏给 MiniUHT 看。编译器这边先当成空宏，
// 真正的 GENERATED_BODY 会在 Xxx.generated.h 里被重新定义。

#ifndef UCLASS
#define UCLASS(...)
#endif

#ifndef UPROPERTY
#define UPROPERTY(...)
#endif

#ifndef UFUNCTION
#define UFUNCTION(...)
#endif

#ifndef GENERATED_BODY
#define GENERATED_BODY()
#endif
