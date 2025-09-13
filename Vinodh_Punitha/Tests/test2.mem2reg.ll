; ModuleID = 'Tests/test2.mem2reg.bc'
source_filename = "Tests/test2.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @foo(i32 noundef %a, i32 noundef %b, i32 noundef %c) #0 {
entry:
  %add = add nsw i32 %a, %b
  %cmp = icmp sgt i32 %c, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %mul = mul nsw i32 %a, %b
  %add1 = add nsw i32 %add, 1
  br label %if.end

if.else:                                          ; preds = %entry
  %mul2 = mul nsw i32 %a, %b
  %sub = sub nsw i32 %add, 1
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %y.0 = phi i32 [ %mul, %if.then ], [ %mul2, %if.else ]
  %z.0 = phi i32 [ %add1, %if.then ], [ %sub, %if.else ]
  %add3 = add nsw i32 %y.0, %z.0
  ret i32 %add3
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
