; ModuleID = 'fib_bench_false-unreach-call.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

%union.pthread_attr_t = type { i64, [48 x i8] }

@i = global i32 1, align 4
@j = global i32 1, align 4

; Function Attrs: noreturn nounwind uwtable
define void @__VERIFIER_error() #0 {
  br label %1

; <label>:1                                       ; preds = %0, %1
  br label %1
                                                  ; No predecessors!
  ret void
}

; Function Attrs: nounwind uwtable
define i8* @t1(i8* %arg) #1 {
  %1 = alloca i8*, align 8
  %k = alloca i32, align 4
  store i8* %arg, i8** %1, align 8
  store i32 0, i32* %k, align 4
  store i32 0, i32* %k, align 4
  br label %2

; <label>:2                                       ; preds = %9, %0
  %3 = load i32* %k, align 4
  %4 = icmp ult i32 %3, 5
  br i1 %4, label %5, label %12

; <label>:5                                       ; preds = %2
  %6 = load i32* @j, align 4
  %7 = load i32* @i, align 4
  %8 = add i32 %7, %6
  store i32 %8, i32* @i, align 4
  br label %9

; <label>:9                                       ; preds = %5
  %10 = load i32* %k, align 4
  %11 = add i32 %10, 1
  store i32 %11, i32* %k, align 4
  br label %2

; <label>:12                                      ; preds = %2
  ret i8* null
}

; Function Attrs: nounwind uwtable
define i8* @t2(i8* %arg) #1 {
  %1 = alloca i8*, align 8
  %k = alloca i32, align 4
  store i8* %arg, i8** %1, align 8
  store i32 0, i32* %k, align 4
  store i32 0, i32* %k, align 4
  br label %2

; <label>:2                                       ; preds = %9, %0
  %3 = load i32* %k, align 4
  %4 = icmp ult i32 %3, 5
  br i1 %4, label %5, label %12

; <label>:5                                       ; preds = %2
  %6 = load i32* @i, align 4
  %7 = load i32* @j, align 4
  %8 = add i32 %7, %6
  store i32 %8, i32* @j, align 4
  br label %9

; <label>:9                                       ; preds = %5
  %10 = load i32* %k, align 4
  %11 = add i32 %10, 1
  store i32 %11, i32* %k, align 4
  br label %2

; <label>:12                                      ; preds = %2
  ret i8* null
}

; Function Attrs: nounwind uwtable
define i32 @main() #1 {
  %1 = alloca i32, align 4
  %id1 = alloca i64, align 8
  %id2 = alloca i64, align 8
  store i32 0, i32* %1
  %2 = call i32 @pthread_create(i64* %id1, %union.pthread_attr_t* null, i8* (i8*)* @t1, i8* null) #3
  %3 = call i32 @pthread_create(i64* %id2, %union.pthread_attr_t* null, i8* (i8*)* @t2, i8* null) #3
  %4 = load i32* @i, align 4
  %5 = icmp uge i32 %4, 144
  br i1 %5, label %9, label %6

; <label>:6                                       ; preds = %0
  %7 = load i32* @j, align 4
  %8 = icmp uge i32 %7, 144
  br i1 %8, label %9, label %11

; <label>:9                                       ; preds = %6, %0
  br label %10

; <label>:10                                      ; preds = %9
  call void @__VERIFIER_error() #4
  unreachable

; <label>:11                                      ; preds = %6
  ret i32 0
}

; Function Attrs: nounwind
declare i32 @pthread_create(i64*, %union.pthread_attr_t*, i8* (i8*)*, i8*) #2

attributes #0 = { noreturn nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }
attributes #4 = { noreturn }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.5.0 (tags/RELEASE_350/final)"}
