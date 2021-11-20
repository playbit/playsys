#include "hello.h"


err_t ioring_req_readfile(
  p_ioring_params_t p,
  void* ring_sq,
  void* ring_sqe,
  const char* filename)
{
  u32 index = 0, current_block = 0, tail = 0, next_tail = 0;
  // next_tail = tail = *sring->tail;
  next_tail = tail = *(u32*)(ring_sq + p.sq_off.tail);
  next_tail++;
  p_mbarrier_r();

  // WIP

  return p_err_not_supported;
}


void hello_ioring() {
  // ioring
  // https://github.com/torvalds/linux/tree/v5.15/tools/io_uring
  p_ioring_params_t ringp = {0};
  fd_t ring = p_syscall_ioring_setup(/*entries*/1, &ringp);
  print("ring:  ", ring, "\n");
  check_status(ring, "p_syscall_ioring_setup");
  print("p_syscall_ioring_setup OK\n");

  // map ring buffer memory
  usize sring_size = (usize)ringp.sq_off.array + ringp.sq_entries * sizeof(u32);
  usize cring_size = (usize)ringp.cq_off.cqes + ringp.cq_entries * sizeof(u32);
  usize ring_size = sring_size > cring_size ? sring_size : cring_size; // max
  void* ring_sq = NULL;
  err_t err = p_syscall_mmap(
    &ring_sq, ring_size,
    p_mmap_prot_read | p_mmap_prot_write | p_mmap_shared | p_mmap_populate,
    ring, P_IORING_OFF_SQ_RING);
  check_status(err, "mmap P_IORING_OFF_SQ_RING");

  // map submission queue entries array (completion queue is in ring_sq)
  void* ring_sqe = NULL;
  err = p_syscall_mmap(
    &ring_sqe, ringp.sq_entries * sizeof(p_ioring_sqe_t),
    p_mmap_prot_read | p_mmap_prot_write | p_mmap_shared | p_mmap_populate,
    ring, P_IORING_OFF_SQES);
  check_status(err, "mmap P_IORING_OFF_SQES");

  // request reading a file (WIP; not yet working)
  err = ioring_req_readfile(ringp, ring_sq, ring_sqe, "hello.txt");
  print("ioring_req_readfile => ", p_err_str(err), "\n");
  // check_status(err, "mmap P_IORING_OFF_SQES");

  // close ring
  check_status(close(ring), "close(ring)");
}
