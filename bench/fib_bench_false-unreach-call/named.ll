; ModuleID = 'orig.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

%union.pthread_attr_t = type { i64, [48 x i8] }

@i = global i32 1, align 4
@j = global i32 1, align 4

; Function Attrs: noreturn nounwind uwtable
define void @__VERIFIER_error() #0 {
bb:
  br label %bb1

bb1:                                              ; preds = %bb1, %bb
  br label %bb1

bb2:                                              ; No predecessors!
  ret void
}

; Function Attrs: nounwind uwtable
define i8* @t1(i8* %arg) #1 {
bb:
  %tmp = alloca i8*, align 8
  %k = alloca i32, align 4
  store i8* %arg, i8** %tmp, align 8
  store i32 0, i32* %k, align 4
  store i32 0, i32* %k, align 4
  br label %bb1

bb1:                                              ; preds = %bb8, %bb
  %tmp2 = load i32* %k, align 4
  %tmp3 = icmp ult i32 %tmp2, 5
  br i1 %tmp3, label %bb4, label %bb11

bb4:                                              ; preds = %bb1
  %tmp5 = load i32* @j, align 4
  %tmp6 = load i32* @i, align 4
  %tmp7 = add i32 %tmp6, %tmp5
  store i32 %tmp7, i32* @i, align 4
  br label %bb8

bb8:                                              ; preds = %bb4
  %tmp9 = load i32* %k, align 4
  %tmp10 = add i32 %tmp9, 1
  store i32 %tmp10, i32* %k, align 4
  br label %bb1

bb11:                                             ; preds = %bb1
  ret i8* null
}

; Function Attrs: nounwind uwtable
define i8* @t2(i8* %arg) #1 {
bb:
  %tmp = alloca i8*, align 8
  %k = alloca i32, align 4
  store i8* %arg, i8** %tmp, align 8
  store i32 0, i32* %k, align 4
  store i32 0, i32* %k, align 4
  br label %bb1

bb1:                                              ; preds = %bb8, %bb
  %tmp2 = load i32* %k, align 4
  %tmp3 = icmp ult i32 %tmp2, 5
  br i1 %tmp3, label %bb4, label %bb11

bb4:                                              ; preds = %bb1
  %tmp5 = load i32* @i, align 4
  %tmp6 = load i32* @j, align 4
  %tmp7 = add i32 %tmp6, %tmp5
  store i32 %tmp7, i32* @j, align 4
  br label %bb8

bb8:                                              ; preds = %bb4
  %tmp9 = load i32* %k, align 4
  %tmp10 = add i32 %tmp9, 1
  store i32 %tmp10, i32* %k, align 4
  br label %bb1

bb11:                                             ; preds = %bb1
  ret i8* null
}

; Function Attrs: nounwind uwtable
define i32 @main() #1 {
bb:
  %tmp = alloca i32, align 4
  %id1 = alloca i64, align 8
  %id2 = alloca i64, align 8
  store i32 0, i32* %tmp
  %tmp1 = call i32 @pthread_create(i64* %id1, %union.pthread_attr_t* null, i8* (i8*)* @t1, i8* null) #3
  %tmp2 = call i32 @pthread_create(i64* %id2, %union.pthread_attr_t* null, i8* (i8*)* @t2, i8* null) #3
  %tmp3 = load i32* @i, align 4
  %tmp4 = icmp uge i32 %tmp3, 144
  br i1 %tmp4, label %bb8, label %bb5

bb5:                                              ; preds = %bb
  %tmp6 = load i32* @j, align 4
  %tmp7 = icmp uge i32 %tmp6, 144
  br i1 %tmp7, label %bb8, label %bb10

bb8:                                              ; preds = %bb5, %bb
  br label %bb9

bb9:                                              ; preds = %bb8
  call void @__VERIFIER_error() #4
  unreachable

bb10:                                             ; preds = %bb5
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
