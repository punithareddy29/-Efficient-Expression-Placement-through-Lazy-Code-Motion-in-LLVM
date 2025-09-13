; ModuleID = 'Tests/test_critical_edge.mem2reg.bc'
source_filename = "Tests/test_critical_edge.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_critical_edge_trigger(i32 noundef %a, i32 noundef %b, i32 noundef %c, i32 noundef %p_cond, i32 noundef %q_cond) #0 {
entry:
  %lcm.tmp = add i32 %a, %b
  %cmp = icmp sgt i32 %p_cond, 0
  br i1 %cmp, label %if.then, label %if.end3

if.then:                                          ; preds = %entry
  %lcm.tmp1 = add i32 %a, %b
  %cmp1 = icmp sgt i32 %q_cond, 10
  br i1 %cmp1, label %if.then2, label %if.end

if.then2:                                         ; preds = %if.then
  br label %return

if.end:                                           ; preds = %if.then
  br label %if.end3

if.end3:                                          ; preds = %if.end, %entry
  %x.0 = phi i32 [ %lcm.tmp, %if.end ], [ %c, %entry ]
  %lcm.tmp2 = add i32 %a, %b
  %add5 = add nsw i32 %x.0, %lcm.tmp
  br label %return

return:                                           ; preds = %if.end3, %if.then2
  %retval.0 = phi i32 [ %lcm.tmp, %if.then2 ], [ %add5, %if.end3 ]
  ret i32 %retval.0
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
