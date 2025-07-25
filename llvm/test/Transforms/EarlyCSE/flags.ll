; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -passes=early-cse -earlycse-debug-hash -S < %s | FileCheck %s
; RUN: opt -passes='early-cse<memssa>' -S < %s | FileCheck %s

declare void @use(i1)
declare void @use.ptr(i32, ptr) memory(read)

define void @test1(float %x, float %y) {
; CHECK-LABEL: @test1(
; CHECK-NEXT:    [[CMP1:%.*]] = fcmp oeq float [[Y:%.*]], [[X:%.*]]
; CHECK-NEXT:    call void @use(i1 [[CMP1]])
; CHECK-NEXT:    call void @use(i1 [[CMP1]])
; CHECK-NEXT:    ret void
;
  %cmp1 = fcmp nnan oeq float %y, %x
  %cmp2 = fcmp oeq float %x, %y
  call void @use(i1 %cmp1)
  call void @use(i1 %cmp2)
  ret void
}

declare void @use.i8(ptr)

define void @test_inbounds_program_ub_if_first_gep_poison(ptr %ptr, i64 %n) {
; CHECK-LABEL: @test_inbounds_program_ub_if_first_gep_poison(
; CHECK-NEXT:    [[ADD_PTR_1:%.*]] = getelementptr inbounds i8, ptr [[PTR:%.*]], i64 [[N:%.*]]
; CHECK-NEXT:    call void @use.i8(ptr noundef [[ADD_PTR_1]])
; CHECK-NEXT:    call void @use.i8(ptr [[ADD_PTR_1]])
; CHECK-NEXT:    ret void
;
  %add.ptr.1 = getelementptr inbounds i8, ptr %ptr, i64 %n
  call void @use.i8(ptr noundef %add.ptr.1)
  %add.ptr.2 = getelementptr i8, ptr %ptr, i64 %n
  call void @use.i8(ptr %add.ptr.2)
  ret void
}

define void @test_inbounds_program_not_ub_if_first_gep_poison(ptr %ptr, i64 %n) {
; CHECK-LABEL: @test_inbounds_program_not_ub_if_first_gep_poison(
; CHECK-NEXT:    [[ADD_PTR_1:%.*]] = getelementptr i8, ptr [[PTR:%.*]], i64 [[N:%.*]]
; CHECK-NEXT:    call void @use.i8(ptr [[ADD_PTR_1]])
; CHECK-NEXT:    call void @use.i8(ptr [[ADD_PTR_1]])
; CHECK-NEXT:    ret void
;
  %add.ptr.1 = getelementptr inbounds i8, ptr %ptr, i64 %n
  call void @use.i8(ptr %add.ptr.1)
  %add.ptr.2 = getelementptr i8, ptr %ptr, i64 %n
  call void @use.i8(ptr %add.ptr.2)
  ret void
}

define void @load_both_nonnull(ptr %p) {
; CHECK-LABEL: @load_both_nonnull(
; CHECK-NEXT:    [[V1:%.*]] = load ptr, ptr [[P:%.*]], align 8, !nonnull [[META0:![0-9]+]]
; CHECK-NEXT:    call void @use.ptr(i32 0, ptr [[V1]])
; CHECK-NEXT:    call void @use.ptr(i32 1, ptr [[V1]])
; CHECK-NEXT:    ret void
;
  %v1 = load ptr, ptr %p, !nonnull !{}
  call void @use.ptr(i32 0, ptr %v1)
  %v2 = load ptr, ptr %p, !nonnull !{}
  call void @use.ptr(i32 1, ptr %v2)
  ret void
}

define void @load_first_nonnull(ptr %p) {
; CHECK-LABEL: @load_first_nonnull(
; CHECK-NEXT:    [[V1:%.*]] = load ptr, ptr [[P:%.*]], align 8
; CHECK-NEXT:    call void @use.ptr(i32 0, ptr [[V1]])
; CHECK-NEXT:    call void @use.ptr(i32 1, ptr [[V1]])
; CHECK-NEXT:    ret void
;
  %v1 = load ptr, ptr %p, !nonnull !{}
  call void @use.ptr(i32 0, ptr %v1)
  %v2 = load ptr, ptr %p
  call void @use.ptr(i32 1, ptr %v2)
  ret void
}

define void @load_first_nonnull_noundef(ptr %p) {
; CHECK-LABEL: @load_first_nonnull_noundef(
; CHECK-NEXT:    [[V1:%.*]] = load ptr, ptr [[P:%.*]], align 8, !nonnull [[META0]], !noundef [[META0]]
; CHECK-NEXT:    call void @use.ptr(i32 0, ptr [[V1]])
; CHECK-NEXT:    call void @use.ptr(i32 1, ptr [[V1]])
; CHECK-NEXT:    ret void
;
  %v1 = load ptr, ptr %p, !nonnull !{}, !noundef !{}
  call void @use.ptr(i32 0, ptr %v1)
  %v2 = load ptr, ptr %p
  call void @use.ptr(i32 1, ptr %v2)
  ret void
}

define ptr @store_to_load_forward(ptr %p, ptr %p2) {
; CHECK-LABEL: @store_to_load_forward(
; CHECK-NEXT:    [[P3:%.*]] = load ptr, ptr [[P:%.*]], align 8, !nonnull [[META0]]
; CHECK-NEXT:    store ptr [[P3]], ptr [[P2:%.*]], align 8
; CHECK-NEXT:    ret ptr [[P3]]
;
  %p3 = load ptr, ptr %p, !nonnull !{}
  store ptr %p3, ptr %p2
  %res = load ptr, ptr %p2
  ret ptr %res
}

define i32 @load_undef_noundef(ptr %p) {
; CHECK-LABEL: @load_undef_noundef(
; CHECK-NEXT:    store i32 undef, ptr [[P:%.*]], align 4
; CHECK-NEXT:    ret i32 undef
;
  store i32 undef, ptr %p
  %v = load i32, ptr %p, !noundef !{}
  ret i32 %v
}
