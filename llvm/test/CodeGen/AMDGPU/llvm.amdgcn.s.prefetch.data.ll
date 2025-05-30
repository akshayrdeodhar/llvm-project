; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 5
; RUN: llc -global-isel=0 -mtriple=amdgcn -mcpu=gfx1200 < %s | FileCheck --check-prefixes=GCN,SDAG %s
; RUN: llc -global-isel=1 -mtriple=amdgcn -mcpu=gfx1200 < %s | FileCheck --check-prefixes=GCN,GISEL %s

define amdgpu_ps void @prefetch_data_sgpr_base_sgpr_len(ptr addrspace(4) inreg %ptr, i32 inreg %len) {
; GCN-LABEL: prefetch_data_sgpr_base_sgpr_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, s2, 0
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 %len)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_imm_base_sgpr_len(ptr addrspace(4) inreg %ptr, i32 inreg %len) {
; GCN-LABEL: prefetch_data_sgpr_imm_base_sgpr_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x200, s2, 0
; GCN-NEXT:    s_endpgm
entry:
  %gep = getelementptr i32, ptr addrspace(4) %ptr, i32 128
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %gep, i32 %len)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_base_imm_len(ptr addrspace(4) inreg %ptr) {
; GCN-LABEL: prefetch_data_sgpr_base_imm_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, null, 31
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 31)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_imm_base_imm_len(ptr addrspace(4) inreg %ptr) {
; GCN-LABEL: prefetch_data_sgpr_imm_base_imm_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x200, null, 31
; GCN-NEXT:    s_endpgm
entry:
  %gep = getelementptr i32, ptr addrspace(4) %ptr, i32 128
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %gep, i32 31)
  ret void
}

define amdgpu_ps void @prefetch_data_vgpr_base_sgpr_len(ptr addrspace(4) %ptr, i32 inreg %len) {
; GCN-LABEL: prefetch_data_vgpr_base_sgpr_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    v_readfirstlane_b32 s2, v0
; GCN-NEXT:    v_readfirstlane_b32 s3, v1
; GCN-NEXT:    s_prefetch_data s[2:3], 0x0, s0, 0
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 %len)
  ret void
}

define amdgpu_ps void @prefetch_data_vgpr_imm_base_sgpr_len(ptr addrspace(4) %ptr, i32 inreg %len) {
; SDAG-LABEL: prefetch_data_vgpr_imm_base_sgpr_len:
; SDAG:       ; %bb.0: ; %entry
; SDAG-NEXT:    v_readfirstlane_b32 s2, v0
; SDAG-NEXT:    v_readfirstlane_b32 s3, v1
; SDAG-NEXT:    s_prefetch_data s[2:3], 0x200, s0, 0
; SDAG-NEXT:    s_endpgm
;
; GISEL-LABEL: prefetch_data_vgpr_imm_base_sgpr_len:
; GISEL:       ; %bb.0: ; %entry
; GISEL-NEXT:    v_add_co_u32 v0, vcc_lo, 0x200, v0
; GISEL-NEXT:    s_delay_alu instid0(VALU_DEP_1) | instskip(NEXT) | instid1(VALU_DEP_2)
; GISEL-NEXT:    v_add_co_ci_u32_e64 v1, null, 0, v1, vcc_lo
; GISEL-NEXT:    v_readfirstlane_b32 s2, v0
; GISEL-NEXT:    s_delay_alu instid0(VALU_DEP_2)
; GISEL-NEXT:    v_readfirstlane_b32 s3, v1
; GISEL-NEXT:    s_prefetch_data s[2:3], 0x0, s0, 0
; GISEL-NEXT:    s_endpgm
entry:
  %gep = getelementptr i32, ptr addrspace(4) %ptr, i32 128
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %gep, i32 %len)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_base_vgpr_len(ptr addrspace(4) inreg %ptr, i32 %len) {
; GCN-LABEL: prefetch_data_sgpr_base_vgpr_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    v_readfirstlane_b32 s2, v0
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, s2, 0
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 %len)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_base_imm_len_global(ptr addrspace(1) inreg %ptr) {
; GCN-LABEL: prefetch_data_sgpr_base_imm_len_global:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, null, 31
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p1(ptr addrspace(1) %ptr, i32 31)
  ret void
}

define amdgpu_ps void @prefetch_data_sgpr_base_imm_len_flat(ptr inreg %ptr) {
; GCN-LABEL: prefetch_data_sgpr_base_imm_len_flat:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, null, 31
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p0(ptr %ptr, i32 31)
  ret void
}

define amdgpu_ps void @prefetch_data_vgpr_base_imm_len(ptr addrspace(4) %ptr) {
; GCN-LABEL: prefetch_data_vgpr_base_imm_len:
; GCN:       ; %bb.0: ; %entry
; GCN-NEXT:    v_readfirstlane_b32 s0, v0
; GCN-NEXT:    v_readfirstlane_b32 s1, v1
; GCN-NEXT:    s_prefetch_data s[0:1], 0x0, null, 0
; GCN-NEXT:    s_endpgm
entry:
  tail call void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 0)
  ret void
}

declare void @llvm.amdgcn.s.prefetch.data.p4(ptr addrspace(4) %ptr, i32 %len)
declare void @llvm.amdgcn.s.prefetch.data.p1(ptr addrspace(1) %ptr, i32 %len)
declare void @llvm.amdgcn.s.prefetch.data.p0(ptr %ptr, i32 %len)
