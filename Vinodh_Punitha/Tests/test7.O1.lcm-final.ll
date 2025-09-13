; ModuleID = 'Tests/test7.O1.mem2reg.bc'
source_filename = "Tests/test7.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local i32 @test7_o0_vs_o1_v2(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %lcm.tmp = add i32 %1, %0
  %lcm.tmp1 = sub i32 %0, %1
  %lcm.tmp2 = mul i32 %1, %0
  %5 = icmp sgt i32 %2, 0
  br i1 %5, label %6, label %10

6:                                                ; preds = %4
  %7 = icmp sgt i32 %3, 5
  br i1 %7, label %8, label %9

8:                                                ; preds = %6
  %lcm.tmp3 = add i32 %1, %0
  br label %11

9:                                                ; preds = %6
  %lcm.tmp4 = sub i32 %0, %1
  br label %11

10:                                               ; preds = %4
  %lcm.tmp5 = mul i32 %1, %0
  br label %11

11:                                               ; preds = %8, %9, %10
  %12 = phi i32 [ %lcm.tmp, %8 ], [ %lcm.tmp1, %9 ], [ %lcm.tmp2, %10 ]
  %lcm.tmp6 = add i32 %12, 100
  ret i32 %lcm.tmp6
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 17.0.6 (++20231209124227+6009708b4367-1~exp1~20231209124336.77)"}
