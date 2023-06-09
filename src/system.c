/* system.c - System support functions.
 * Copyright (C) 2009 Free Software Foundation, Inc.
 *
 * This file is part of Assuan.
 *
 * Assuan is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Assuan is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1+
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
  /* Solaris 8 needs sys/types.h before time.h.  */
# include <sys/types.h>
#endif
#include <time.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "assuan-defs.h"
#include "debug.h"

#define DEBUG_SYSIO 0




/* Manage memory specific to a context.  */

void *
_assuan_malloc (assuan_context_t ctx, size_t cnt)
{
  return ctx->malloc_hooks.malloc (cnt);
}

void *
_assuan_realloc (assuan_context_t ctx, void *ptr, size_t cnt)
{
  return ctx->malloc_hooks.realloc (ptr, cnt);
}

void *
_assuan_calloc (assuan_context_t ctx, size_t cnt, size_t elsize)
{
  void *ptr;
  size_t nbytes;

  nbytes = cnt * elsize;

  /* Check for overflow.  */
  if (elsize && nbytes / elsize != cnt)
    {
      gpg_err_set_errno (ENOMEM);
      return NULL;
    }

  ptr = ctx->malloc_hooks.malloc (nbytes);
  if (ptr)
    memset (ptr, 0, nbytes);
  return ptr;
}

void
_assuan_free (assuan_context_t ctx, void *ptr)
{
  if (ptr)
    ctx->malloc_hooks.free (ptr);
}


/* Release the memory at PTR using the allocation handler of the
   context CTX.  This is a convenience function.  */
void
assuan_free (assuan_context_t ctx, void *ptr)
{
  _assuan_free (ctx, ptr);
}



/* Copy the system hooks struct, paying attention to version
   differences.  SRC is usually from the user, DST MUST be from the
   library.  */
void
_assuan_system_hooks_copy (assuan_system_hooks_t dst,
			   assuan_system_hooks_t src)

{
  if (src == NULL)
    return;

  /* Reset the defaults.  */
  if (dst != &_assuan_system_hooks)
    memcpy (dst, &_assuan_system_hooks, sizeof (*dst));

  dst->version = ASSUAN_SYSTEM_HOOKS_VERSION;
  if (src->version >= 1)
    {
      dst->usleep = src->usleep;
      dst->pipe = src->pipe;
      dst->close = src->close;
      dst->read = src->read;
      dst->write = src->write;
      dst->sendmsg = src->sendmsg;
      dst->recvmsg = src->recvmsg;
      dst->spawn = src->spawn;
      dst->waitpid = src->waitpid;
      dst->socketpair = src->socketpair;
    }
  if (src->version >= 2)
    {
      dst->socket = src->socket;
      dst->connect = src->connect;
    }
  if (src->version > 2)
    /* FIXME.  Application uses newer version of the library.  What to
       do?  */
    ;
}



/* Sleep for the given number of microseconds.  */
void
_assuan_usleep (assuan_context_t ctx, unsigned int usec)
{
  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "_assuan_usleep", ctx,
	  "usec=%u", usec);

  if (ctx->system.version)
    (ctx->system.usleep) (ctx, usec);
  else
    {
      _assuan_pre_syscall ();
      __assuan_usleep (ctx, usec);
      _assuan_post_syscall ();
    }
}



/* Create a pipe with one inheritable end.  */
int
_assuan_pipe (assuan_context_t ctx, assuan_fd_t fd[2], int inherit_idx)
{
  int err;
  TRACE_BEG2 (ctx, ASSUAN_LOG_SYSIO, "_assuan_pipe", ctx,
	      "inherit_idx=%i (Assuan uses it for %s)",
	      inherit_idx, inherit_idx ? "reading" : "writing");

  if (ctx->system.version)
    err = (ctx->system.pipe) (ctx, fd, inherit_idx);
  else
    err = __assuan_pipe (ctx, fd, inherit_idx);
  if (err)
    return TRACE_SYSRES (err);

  return TRACE_SUC2 ("read=0x%x, write=0x%x", fd[0], fd[1]);
}



/* Close the given file descriptor, created with _assuan_pipe or one
   of the socket functions.  */
int
_assuan_close (assuan_context_t ctx, assuan_fd_t fd)
{
  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "_assuan_close", ctx,
	  "fd=0x%x", fd);

  if (ctx->system.version)
    return (ctx->system.close) (ctx, fd);
  else
    {
      int res;
      _assuan_pre_syscall ();
      res = __assuan_close (ctx, fd);
      _assuan_post_syscall ();
      return res;
    }
}


/* Same as assuan_close but used for the inheritable end of a
   pipe.  */
int
_assuan_close_inheritable (assuan_context_t ctx, assuan_fd_t fd)
{
  TRACE1 (ctx, ASSUAN_LOG_SYSIO, "_assuan_close_inheritable", ctx,
	  "fd=0x%x", fd);

  if (ctx->system.version)
    return (ctx->system.close) (ctx, fd);
  else
    {
      int res;
      _assuan_pre_syscall ();
      res = __assuan_close (ctx, fd);
      _assuan_post_syscall ();
      return res;
    }
}



ssize_t
_assuan_read (assuan_context_t ctx, assuan_fd_t fd, void *buffer, size_t size)
{
#if DEBUG_SYSIO
  ssize_t res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_read", ctx,
	      "fd=0x%x, buffer=%p, size=%i", fd, buffer, size);
  if (ctx->system.version)
    res = (ctx->system.read) (ctx, fd, buffer, size);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_read (ctx, fd, buffer, size);
      _assuan_post_syscall ();
    }
  return TRACE_SYSRES (res);
#else
  if (ctx->system.version)
    return (ctx->system.read) (ctx, fd, buffer, size);
  else
    {
      ssize_t res;
      _assuan_pre_syscall ();
      res = __assuan_read (ctx, fd, buffer, size);
      _assuan_post_syscall ();
      return res;
    }
#endif
}



ssize_t
_assuan_write (assuan_context_t ctx, assuan_fd_t fd, const void *buffer,
	       size_t size)
{
#if DEBUG_SYSIO
  ssize_t res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_write", ctx,
	      "fd=0x%x, buffer=%p, size=%i", fd, buffer, size);
  if (ctx->system.version)
    res = (ctx->system.write) (ctx, fd, buffer, size);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_write (ctx, fd, buffer, size);
      _assuan_post_syscall ();
    }
  return TRACE_SYSRES (res);
#else
  if (ctx->system.version)
    return (ctx->system.write) (ctx, fd, buffer, size);
  else
    {
      ssize_t res;
      _assuan_pre_syscall ();
      res =  __assuan_write (ctx, fd, buffer, size);
      _assuan_post_syscall ();
      return res;
    }
#endif
}



int
_assuan_recvmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		 int flags)
{
#if DEBUG_SYSIO
  int res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_recvmsg", ctx,
	      "fd=0x%x, msg=%p, flags=0x%x", fd, msg, flags);
  if (ctx->system.version)
    res = (ctx->system.recvmsg) (ctx, fd, msg, flags);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_recvmsg (ctx, fd, msg, flags);
      _assuan_post_syscall ();
    }
  if (res > 0)
    {
      struct cmsghdr *cmptr;

      TRACE_LOG2 ("msg->msg_iov[0] = { iov_base=%p, iov_len=%i }",
		  msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);
      TRACE_LOGBUF (msg->msg_iov[0].iov_base, res);

      cmptr = CMSG_FIRSTHDR (msg);
      if (cmptr)
	{
	  void *data = CMSG_DATA (cmptr);
	  TRACE_LOG5 ("cmsg_len=0x%x (0x%x data), cmsg_level=0x%x, "
		      "cmsg_type=0x%x, first data int=0x%x", cmptr->cmsg_len,
		      cmptr->cmsg_len - (((char *)data) - ((char *)cmptr)),
		      cmptr->cmsg_level, cmptr->cmsg_type, *(int *)data);
	}
    }
  return TRACE_SYSRES (res);
#else
  if (ctx->system.version)
    return (ctx->system.recvmsg) (ctx, fd, msg, flags);
  else
    {
      int res;
      _assuan_pre_syscall ();
      res = __assuan_recvmsg (ctx, fd, msg, flags);
      _assuan_post_syscall ();
      return res;
    }
#endif
}



int
_assuan_sendmsg (assuan_context_t ctx, assuan_fd_t fd, assuan_msghdr_t msg,
		 int flags)
{
#if DEBUG_SYSIO
  int res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_sendmsg", ctx,
	      "fd=0x%x, msg=%p, flags=0x%x", fd, msg, flags);
  {
    struct cmsghdr *cmptr;

    TRACE_LOG2 ("msg->iov[0] = { iov_base=%p, iov_len=%i }",
		msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);
    TRACE_LOGBUF (msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);

    cmptr = CMSG_FIRSTHDR (msg);
    if (cmptr)
      {
	void *data = CMSG_DATA (cmptr);
	TRACE_LOG5 ("cmsg_len=0x%x (0x%x data), cmsg_level=0x%x, "
		    "cmsg_type=0x%x, first data int=0x%x", cmptr->cmsg_len,
		    cmptr->cmsg_len - (((char *)data) - ((char *)cmptr)),
		    cmptr->cmsg_level, cmptr->cmsg_type, *(int *)data);
      }
  }
  if (ctx->system.version)
    res = (ctx->system.sendmsg) (ctx, fd, msg, flags);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_sendmsg (ctx, fd, msg, flags);
      _assuan_post_syscall ();
    }
  return TRACE_SYSRES (res);
#else
  if (ctx->system.version)
    return (ctx->system.sendmsg) (ctx, fd, msg, flags);
  else
    {
      int res;
      _assuan_pre_syscall ();
      res = __assuan_sendmsg (ctx, fd, msg, flags);
      _assuan_post_syscall ();
      return res;
    }
#endif
}



/* Create a new process from NAME and ARGV.  Provide FD_IN and FD_OUT
   as stdin and stdout.  Inherit the ASSUAN_INVALID_FD-terminated
   FD_CHILD_LIST as given (no remapping), which must be inheritable.
   On Unix, call ATFORK with ATFORKVALUE after fork and before exec.  */
int
_assuan_spawn (assuan_context_t ctx, assuan_pid_t *r_pid, const char *name,
	       const char **argv,
	       assuan_fd_t fd_in, assuan_fd_t fd_out,
	       assuan_fd_t *fd_child_list,
	       void (*atfork) (void *opaque, int reserved),
	       void *atforkvalue, unsigned int flags)
{
  int res;
  int i;
  TRACE_BEG6 (ctx, ASSUAN_LOG_CTX, "_assuan_spawn", ctx,
	      "name=%s,fd_in=0x%x,fd_out=0x%x,"
	      "atfork=%p,atforkvalue=%p,flags=%i",
	      name ? name : "(null)", fd_in, fd_out,
	      atfork, atforkvalue, flags);

  if (name)
    {
      i = 0;
      while (argv[i])
	{
	  TRACE_LOG2 ("argv[%2i] = %s", i, argv[i]);
	  i++;
	}
    }
  i = 0;
  if (fd_child_list)
    {
      while (fd_child_list[i] != ASSUAN_INVALID_FD)
	{
	  TRACE_LOG2 ("fd_child_list[%2i] = 0x%x", i, fd_child_list[i]);
	  i++;
	}
    }

  if (ctx->system.version)
    res = (ctx->system.spawn) (ctx, r_pid, name, argv, fd_in, fd_out,
                               fd_child_list, atfork, atforkvalue, flags);
  else
    res = __assuan_spawn (ctx, r_pid, name, argv, fd_in, fd_out,
                          fd_child_list, atfork, atforkvalue, flags);

  if (name)
    {
      TRACE_LOG1 ("pid = 0x%x", *r_pid);
    }
  else
    {
      TRACE_LOG2 ("pid = 0x%x (%s)", *r_pid, *argv);
    }

  return TRACE_SYSERR (res);
}



/* FIXME: Add some sort of waitpid function that covers GPGME and
   gpg-agent's use of assuan.  */
assuan_pid_t
_assuan_waitpid (assuan_context_t ctx, assuan_pid_t pid, int action,
		 int *status, int options)
{
#if DEBUG_SYSIO
  assuan_pid_t res;
  TRACE_BEG4 (ctx, ASSUAN_LOG_SYSIO, "_assuan_waitpid", ctx,
	      "pid=%i, action=%i, status=%p, options=%i",
	      pid, action, status, options);
  if (ctx->system.version)
    res = (ctx->system.waitpid) (ctx, pid, action, status, options);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_waitpid (ctx, pid, action, status, options);
      _assuan_post_syscall ();
    }
  return TRACE_SYSRES (res);
#else
  if (ctx->system.version)
    return (ctx->system.waitpid) (ctx, pid, action, status, options);
  else
    {
      assuan_pid_t res;
      _assuan_pre_syscall ();
      res = __assuan_waitpid (ctx, pid, action, status, options);
      _assuan_post_syscall ();
      return res;
    }
#endif
}



int
_assuan_socketpair (assuan_context_t ctx, int namespace, int style,
		    int protocol, assuan_fd_t filedes[2])
{
  int res;
  TRACE_BEG4 (ctx, ASSUAN_LOG_SYSIO, "_assuan_socketpair", ctx,
	      "namespace=%i,style=%i,protocol=%i,filedes=%p",
	      namespace, style, protocol, filedes);

  if (ctx->system.version)
    res = (ctx->system.socketpair) (ctx, namespace, style, protocol, filedes);
  else
    res = __assuan_socketpair (ctx, namespace, style, protocol, filedes);
  if (res == 0)
    TRACE_LOG2 ("filedes = { 0x%x, 0x%x }", filedes[0], filedes[1]);

  return TRACE_SYSERR (res);
}



assuan_fd_t
_assuan_socket (assuan_context_t ctx, int namespace, int style, int protocol)
{
  assuan_fd_t res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_socket", ctx,
	      "namespace=%i,style=%i,protocol=%i",
	      namespace, style, protocol);

  if (ctx->system.version)
    res = (ctx->system.socket) (ctx, namespace, style, protocol);
  else
    res = __assuan_socket (ctx, namespace, style, protocol);
  return TRACE_SYSRES (res);
}


int
_assuan_connect (assuan_context_t ctx, assuan_fd_t sock,
                 struct sockaddr *addr, socklen_t length)
{
  int res;
  TRACE_BEG3 (ctx, ASSUAN_LOG_SYSIO, "_assuan_connect", ctx,
	      "socket=%i,addr=%p,length=%i", sock, addr, length);

  if (ctx->system.version)
    res = (ctx->system.connect) (ctx, sock, addr, length);
  else
    {
      _assuan_pre_syscall ();
      res = __assuan_connect (ctx, sock, addr, length);
      _assuan_post_syscall ();
    }
  return TRACE_SYSRES (res);
}
