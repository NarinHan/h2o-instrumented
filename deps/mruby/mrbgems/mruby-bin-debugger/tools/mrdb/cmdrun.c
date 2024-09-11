#ifndef LOGGING_H
#define LOGGING_H
#include "logging.h"
#endif

/*
** cmdrun.c - mruby debugger run command functions
**
*/

#include <mruby/opcode.h>
#include "mrdb.h"

dbgcmd_state
dbgcmd_run(mrb_state *mrb, mrdb_state *mrdb)
{
  mrb_debug_context *dbg = mrdb->dbg;

  if (dbg->xm == DBG_INIT){
    {  // Begin logged block
    dbg->xm = DBG_RUN;
    LOG_VAR_INT(dbg->xm); // Auto-logged
    }  // End logged block
  }
  else {
    {  // Begin logged block
    dbg->xm = DBG_QUIT;
    LOG_VAR_INT(dbg->xm); // Auto-logged
    }  // End logged block
    if (dbg->xphase == DBG_PHASE_RUNNING){
      struct RClass *exc;
      puts("Start it from the beginning");
      exc = mrb_define_class(mrb, "DebuggerRestart", mrb->eException_class);
      mrb_raise(mrb, exc, "Restart mrdb");
    }
  }

  return DBGST_RESTART;
}

dbgcmd_state
dbgcmd_continue(mrb_state *mrb, mrdb_state *mrdb)
{
  mrb_debug_context *dbg = mrdb->dbg;
  int ccnt = 1;

  if (mrdb->wcnt > 1){
    sscanf(mrdb->words[1], "%d", &ccnt);
  }
  dbg->ccnt = (uint16_t)(ccnt > 0 ? ccnt : 1);  /* count of continue */

  if (dbg->xphase == DBG_PHASE_AFTER_RUN){
    puts("The program is not running.");
    {  // Begin logged block
    dbg->xm = DBG_QUIT;
    LOG_VAR_INT(dbg->xm); // Auto-logged
    }  // End logged block
  }
  else {
    {  // Begin logged block
    dbg->xm = DBG_RUN;
    LOG_VAR_INT(dbg->xm); // Auto-logged
    }  // End logged block
  }
  return DBGST_CONTINUE;
}

dbgcmd_state
dbgcmd_step(mrb_state *mrb, mrdb_state *mrdb)
{
    {  // Begin logged block
    mrdb->dbg->xm = DBG_STEP;
    LOG_VAR_INT(mrdb->dbg->xm); // Auto-logged
    }  // End logged block
  return DBGST_CONTINUE;
}

dbgcmd_state
dbgcmd_next(mrb_state *mrb, mrdb_state *mrdb)
{
    {  // Begin logged block
    mrdb->dbg->xm = DBG_NEXT;
    LOG_VAR_INT(mrdb->dbg->xm); // Auto-logged
    }  // End logged block
  mrdb->dbg->prvci = mrb->c->ci;
  return DBGST_CONTINUE;
}
