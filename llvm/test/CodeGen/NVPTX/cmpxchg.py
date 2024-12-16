from string import Template

direct_expansion = Template(
    """
    define i$size @${success}_${failure}_i${size}(ptr %addr, i$size %cmp, i$size %new) {
        ; $prefix-LABEL: ${success}_${failure}_i${size}(
        ; $prefix: $leadingfence
        ; $prefix: $cas
        ; $prefix: $trailingfence
        %pairold = cmpxchg ptr %addr, i$size %cmp, i$size %new $success $failure
        ret i$size %new
    }
    """
)

emulated_expansion = Template(
    # todo: replace the ld.u$size with "load"
    """
    define i$size @${success}_${failure}_i${size}(ptr %addr, i$size %cmp, i$size %new) {
        ; $prefix-LABEL: ${success}_${failure}_i${size}(
        ; $prefix: $leadingfence
        ; $prefix: ld.u32
        ; $prefix: // %partword.cmpxchg.loop
        ; $prefix: $cas
        ; $prefix: // %partword.cmpxchg.failure
        ; $prefix: // %partword.cmpxchg.end
        ; $prefix: $trailingfence
        %pairold = cmpxchg ptr %addr, i$size %cmp, i$size %new $success $failure
        ret i$size %new
    }
    """
)

fence_inst = Template("fence.$sem.$scope")
cas_inst = Template("atom.$sem.cas.b$size")
relaxed_cas_inst = Template("atom.cas.b$size")

ordering_strength = {
    "monotonic": 0,
    "acquire": 1,
    "release": 1,
    "acq_rel": 2,
    "seq_cst": 3
}

llvm_to_ptx_order = {
    "monotonic": "relaxed",
    "acquire": "acquire",
    "release": "release",
    "acq_rel": "acq_rel",
    "seq_cst": "sc"
}

def get_merged_ordering(success, failure):
    if success == "acquire" and failure == "release":
        return "acq_rel"
    elif success == "release" and failure == "acquire":
        return "acq_rel"
    else:
        if ordering_strength[success] > ordering_strength[failure]:
            return success
        else:
            return failure

def get_fence_inst(ordering, scope):
    ptx_ordering = llvm_to_ptx_order[ordering]
    return fence_inst.substitute(sem = llvm_to_ptx_order[ordering], scope = "sys")

def get_cas_inst(ordering, size):
    return cas_inst.substitute(sem = llvm_to_ptx_order[ordering], size = str(size))

def get_direct_expansion(size, success_ordering, failure_ordering):
    merged_ordering = get_merged_ordering(success_ordering, failure_ordering)
    if merged_ordering == "seq_cst":
        leadingfence = get_fence_inst("seq_cst", "sys")
        cas = get_cas_inst("acquire", size)
        trailingfence = "{{.*}}"
        return direct_expansion.substitute(size = size, success = success_ordering, 
                                           failure = failure_ordering, leadingfence = leadingfence,
                                           cas = cas, trailingfence = trailingfence, prefix = "CHECK")
    else:
        cas = cas_inst.substitute(size = size, sem = llvm_to_ptx_order[merged_ordering])
        return direct_expansion.substitute(size = size, success = success_ordering,
                                           failure = failure_ordering, leadingfence = "{{.*}}",
                                           cas = cas, trailingfence = "{{.*}}", prefix = "CHECK")


def get_leading_fence(success_ordering, failure_ordering):
    merged_ordering = get_merged_ordering(success_ordering, failure_ordering)
    if merged_ordering == "seq_cst":
        return fence_inst.substitute(sem = llvm_to_ptx_order["seq_cst"], scope = "sys")
    elif ordering_strength[merged_ordering] >= ordering_strength["release"] and not merged_ordering == "acquire":
        return fence_inst.substitute(sem = llvm_to_ptx_order["release"], scope = "sys")
    else:
        return "{{.*}}"

def get_trailing_fence(success_ordering, failure_ordering):
    merged_ordering = get_merged_ordering(success_ordering, failure_ordering)
    if ordering_strength[merged_ordering] >= ordering_strength["acquire"] and not merged_ordering == "release":
        return fence_inst.substitute(sem = llvm_to_ptx_order["acquire"], scope = "sys")
    else:
        return "{{.*}}"

def get_emulated_expansion(size, success_ordering, failure_ordering):
    leading_fence = get_leading_fence(success_ordering, failure_ordering)
    trailing_fence = get_trailing_fence(success_ordering, failure_ordering)
    cas = relaxed_cas_inst.substitute(size = "32")
    return emulated_expansion.substitute(size = size, success = success_ordering,
                                         failure = failure_ordering, leadingfence = leading_fence,
                                         cas = cas, trailingfence = trailing_fence, prefix = "CHECK")



run_line = """
    ; RUN: llc < %s -march=nvptx64 -mcpu=sm_90 -mattr=+ptx86 | FileCheck %s
    ; RUN: %if ptxas %{ llc < %s -march=nvptx64 -mcpu=sm_90 -mattr=+ptx86 | %ptxas-verify -arch=sm_90 %}
    """
print(run_line)

for size in [8, 16, 32, 64]:
    for success_ordering in ["monotonic", "acquire", "release", "acq_rel", "seq_cst"]:
        for failure_ordering in ["monotonic", "acquire", "seq_cst"]:
            if size >= 32:
                print(get_direct_expansion(size, success_ordering, failure_ordering))
            else:
                print(get_emulated_expansion(size, success_ordering, failure_ordering))
