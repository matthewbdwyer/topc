; ModuleID = 'test/system/iotests/fib.top'
source_filename = "test/system/iotests/fib.top"
target triple = "arm64-apple-darwin24.6.0"

@_top_ftable = internal constant [2 x ptr] [ptr @fib, ptr @_top_main]
@_top_num_inputs = constant i64 1
@_top_input_array = common global [1 x i64] zeroinitializer

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(none)
declare void @llvm.donothing() #0

define internal i64 @fib(i64 %n) {
entry:
  br label %header1

header1:                                          ; preds = %body1, %entry
  %f1.0 = phi i64 [ 1, %entry ], [ %f2.0, %body1 ]
  %f2.0 = phi i64 [ 1, %entry ], [ %add, %body1 ]
  %i.0 = phi i64 [ %n, %entry ], [ %subtract, %body1 ]
  %compare.gt = icmp sgt i64 %i.0, 1
  br i1 %compare.gt, label %body1, label %exit1

body1:                                            ; preds = %header1
  %add = add i64 %f2.0, %f1.0
  %subtract = add nsw i64 %i.0, -1
  br label %header1

exit1:                                            ; preds = %header1
  ret i64 %f2.0
}

define i64 @_top_main() {
entry:
  %topinput0 = load i64, ptr @_top_input_array, align 4
  %call.result = call i64 @fib(i64 %topinput0)
  ret i64 %call.result
}

; Function Attrs: nounwind
declare noalias ptr @calloc(i64, i64) #1

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(none) }
attributes #1 = { nounwind }
