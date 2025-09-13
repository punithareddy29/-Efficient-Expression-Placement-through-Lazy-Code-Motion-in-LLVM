; ModuleID = 'Tests/test_complex_cfg.mem2reg.bc'
source_filename = "Tests/test_complex_cfg.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test_complex_cfg(i32 noundef %a, i32 noundef %b, i32 noundef %c, i32 noundef %d) #0 {
entry:
  %lcm.tmp = add i32 %a, %b
  %lcm.tmp1 = sub i32 %c, %d
  %cmp = icmp sgt i32 %a, 10
  br i1 %cmp, label %if.then, label %if.else8

if.then:                                          ; preds = %entry
  %add1 = add nsw i32 %c, %d
  %cmp2 = icmp sgt i32 %b, 5
  br i1 %cmp2, label %if.then3, label %if.else

if.then3:                                         ; preds = %if.then
  %lcm.tmp2 = mul i32 %lcm.tmp, 2
  %add5 = add nsw i32 %add1, %lcm.tmp
  br label %if.end

if.else:                                          ; preds = %if.then
  %lcm.tmp3 = sub i32 %c, %d
  %sub7 = sub nsw i32 %add1, %lcm.tmp
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then3
  %x.0 = phi i32 [ %lcm.tmp2, %if.then3 ], [ %lcm.tmp1, %if.else ]
  %y.0 = phi i32 [ %add5, %if.then3 ], [ %sub7, %if.else ]
  br label %if.end19

if.else8:                                         ; preds = %entry
  %sub9 = sub nsw i32 %a, %b
  %cmp10 = icmp sgt i32 %c, 0
  br i1 %cmp10, label %if.then11, label %if.else15

if.then11:                                        ; preds = %if.else8
  %lcm.tmp4 = add i32 %lcm.tmp, 5
  %mul14 = mul nsw i32 %sub9, %lcm.tmp
  br label %if.end18

if.else15:                                        ; preds = %if.else8
  %lcm.tmp5 = sdiv i32 %sub9, 2
  %mul17 = mul nsw i32 %lcm.tmp, 3
  br label %if.end18

if.end18:                                         ; preds = %if.else15, %if.then11
  %x.1 = phi i32 [ %lcm.tmp4, %if.then11 ], [ %mul17, %if.else15 ]
  %y.1 = phi i32 [ %mul14, %if.then11 ], [ %lcm.tmp5, %if.else15 ]
  br label %if.end19

if.end19:                                         ; preds = %if.end18, %if.end
  %x.2 = phi i32 [ %x.0, %if.end ], [ %x.1, %if.end18 ]
  %y.2 = phi i32 [ %y.0, %if.end ], [ %y.1, %if.end18 ]
  %lcm.tmp6 = add i32 %x.2, %y.2
  %add22 = add nsw i32 %lcm.tmp6, %lcm.tmp
  ret i32 %add22
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
