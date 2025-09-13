; ModuleID = 'Tests/test7.O1.mem2reg.bc'
source_filename = "Tests/test7.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local i32 @test7_o0_vs_o1_v2(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  %5 = icmp sgt i32 %2, 0
  br i1 %5, label %6, label %12

6:                                                ; preds = %4
  %7 = icmp sgt i32 %3, 5
  br i1 %7, label %8, label %10

8:                                                ; preds = %6
  %9 = add nsw i32 %1, %0
  br label %14

10:                                               ; preds = %6
  %11 = sub nsw i32 %0, %1
  br label %14

12:                                               ; preds = %4
  %13 = mul nsw i32 %1, %0
  br label %14

14:                                               ; preds = %8, %10, %12
  %15 = phi i32 [ %9, %8 ], [ %11, %10 ], [ %13, %12 ]
  %16 = add nsw i32 %15, 100
  ret i32 %16
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 17.0.6 (++20231209124227+6009708b4367-1~exp1~20231209124336.77)"}
