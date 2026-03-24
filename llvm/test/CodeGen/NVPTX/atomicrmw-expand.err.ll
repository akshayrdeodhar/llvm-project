; RUN: not llc < %s -mtriple=nvptx64 -mcpu=sm_90 -mattr=+ptx83 -filetype=null 2>&1 | FileCheck %s

; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
define void @bitwise_i256(ptr %0, i256 %1) {
entry:
  %2 = atomicrmw and ptr %0, i256 %1 monotonic
  %3 = atomicrmw or ptr %0, i256 %1 monotonic
  %4 = atomicrmw xor ptr %0, i256 %1 monotonic
  %5 = atomicrmw xchg ptr %0, i256 %1 monotonic
  ret void
}

; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
; CHECK: error: unsupported atomic load
; CHECK: error: unsupported cmpxchg
define void @minmax_i256(ptr %0, i256 %1) {
entry:
  %2 = atomicrmw min ptr %0, i256 %1 monotonic
  %3 = atomicrmw max ptr %0, i256 %1 monotonic
  %4 = atomicrmw umin ptr %0, i256 %1 monotonic
  %5 = atomicrmw umax ptr %0, i256 %1 monotonic
  ret void
}
