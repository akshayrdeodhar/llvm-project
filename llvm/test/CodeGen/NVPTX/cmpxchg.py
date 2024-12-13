
from string import Template

direct_expansion = Template(
    """
    define i$size @$success_$failure_i$size(ptr %addr, i$size %cmp, i$size %new) {
        ; $prefix-LABEL: $success_$failure_i$size(
        ; $prefix: $leadingfence
        ; $prefix: $cas
        ; $prefix: $trailingfence
        %pairold = cmpxchg ptr %addr, i$type %cmp, i$type %new seq_cst seq_cst
        ret i$size %new
    }
    """
)

emulated_expansion = Template(
    """
    define i$size @$success_$failure_i$size(ptr %addr, i$size %cmp, i$size %new) {
        ; $prefix-LABEL: $success_$failure_i$size(
        ; $prefix: $leadingfence
        ; $prefix: $load
        ; $prefix: // %partword.cmpxchg.loop
        ; $prefix: $cas
        ; $prefix: // %partword.cmpxchg.failure
        ; $prefix: // %partword.cmpxchg.end
        ; $prefix: $trailingfence
        %pairold = cmpxchg ptr %addr, i$type %cmp, i$type %new seq_cst seq_cst
        ret i$size %new
    }
    """
)

fence_inst = Template("fence.$sem.$scope")
cas_inst = Template("atom.$sem.cas.b$size")

for size in [8, 16, 32, 64]:
    for success_ordering in ["monotonic", "acquire", "release", "acq_rel", "seq_cst"]:
        for failure_ordering in ["monotonic", "acquire", "seq_cst"]:
            if size > 32:
                # direct expansion here
            else:
                # emulation loop here
