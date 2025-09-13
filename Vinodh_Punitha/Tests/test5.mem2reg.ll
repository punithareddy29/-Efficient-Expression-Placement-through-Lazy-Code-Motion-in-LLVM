; ModuleID = 'Tests/test5.mem2reg.bc'
source_filename = "Tests/test5.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @complex_cfg(i32 noundef %a, i32 noundef %b, i32 noundef %c) #0 {
entry:
  %cmp = icmp sgt i32 %a, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %add = add nsw i32 %b, %c
  %mul = mul nsw i32 %a, 2
  br label %if.end7

if.else:                                          ; preds = %entry
  %cmp1 = icmp sgt i32 %b, 0
  br i1 %cmp1, label %if.then2, label %if.else5

if.then2:                                         ; preds = %if.else
  %add3 = add nsw i32 %b, %c
  %mul4 = mul nsw i32 %a, 3
  br label %if.end

if.else5:                                         ; preds = %if.else
  %sub = sub nsw i32 %b, %c
  %mul6 = mul nsw i32 %a, 4
  br label %if.end

if.end:                                           ; preds = %if.else5, %if.then2
  %x.0 = phi i32 [ %add3, %if.then2 ], [ %sub, %if.else5 ]
  %y.0 = phi i32 [ %mul4, %if.then2 ], [ %mul6, %if.else5 ]
  br label %if.end7

if.end7:                                          ; preds = %if.end, %if.then
  %x.1 = phi i32 [ %add, %if.then ], [ %x.0, %if.end ]
  %y.1 = phi i32 [ %mul, %if.then ], [ %y.0, %if.end ]
  %add8 = add nsw i32 %b, %c
  %add9 = add nsw i32 %x.1, %y.1
  %add10 = add nsw i32 %add9, %add8
  ret i32 %add10
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 17.0.6 (++20231209124227+6009708b4367-1~exp1~20231209124336.77)"}
