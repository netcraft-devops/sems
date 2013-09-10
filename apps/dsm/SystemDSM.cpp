
#include "SystemDSM.h"
#include "log.h"
#include "AmUtils.h"
#include "AmEventDispatcher.h"

#include "DSMStateDiagramCollection.h"
#include "../apps/jsonrpc/JsonRPCEvents.h" // todo!

SystemDSM::SystemDSM(const DSMScriptConfig& config,
		     const string& startDiagName,
		     bool reload) 
  : stop_requested(false), AmEventQueue(this), 
    startDiagName(startDiagName), dummy_session(this), 
    reload(reload)
{
  config.diags->addToEngine(&engine);

  for (map<string, string>::const_iterator it = 
	 config.config_vars.begin(); it != config.config_vars.end(); it++) 
    var["config."+it->first] = it->second;

  // register our event queue
  string our_id = "SystemDSM_"+AmSession::getNewId();
  dummy_session.setLocalTag(our_id);
  AmEventDispatcher::instance()->addEventQueue(our_id, this);
}

SystemDSM::~SystemDSM() {
}

void SystemDSM::run() {
  DBG("SystemDSM thread starting...\n");

  DBG("Running init of SystemDSM...\n");
  if (!engine.init(&dummy_session, this, startDiagName, 
		   reload ? DSMCondition::Reload : DSMCondition::Startup)) {
    WARN("Initialization failed for SystemDSM\n");
    AmEventDispatcher::instance()->delEventQueue(dummy_session.getLocalTag());
    return;
  }

  while (!stop_requested.get() && !dummy_session.getStopped()) {
    waitForEvent();
    processEvents();
  }

  AmEventDispatcher::instance()->delEventQueue(dummy_session.getLocalTag());
  DBG("SystemDSM thread finished.\n");
}

void SystemDSM::on_stop() {
  DBG("requesting stop of SystemDSM\n");
  stop_requested.set(true);
}

void SystemDSM::process(AmEvent* event) {

  AmPluginEvent* plugin_event = dynamic_cast<AmPluginEvent*>(event);
  if(plugin_event && plugin_event->name == "timer_timeout") {
    int timer_id = plugin_event->data.get(0).asInt();
    map<string, string> params;
    params["id"] = int2str(timer_id);
    engine.runEvent(&dummy_session, this, DSMCondition::Timer, &params);
  }

  if (event->event_id == DSM_EVENT_ID) {
    DSMEvent* dsm_event = dynamic_cast<DSMEvent*>(event);
    if (dsm_event) {      
      engine.runEvent(&dummy_session, this, DSMCondition::DSMEvent, &dsm_event->params);
      return;
    }  
  }
  // todo: give modules the possibility to define/process events
  JsonRpcEvent* jsonrpc_ev = dynamic_cast<JsonRpcEvent*>(event);
  if (jsonrpc_ev) { 
    DBG("received jsonrpc event\n");

    JsonRpcResponseEvent* resp_ev = 
      dynamic_cast<JsonRpcResponseEvent*>(jsonrpc_ev);
    if (resp_ev) {
      map<string, string> params;
      params["ev_type"] = "JsonRpcResponse";
      params["id"] = resp_ev->response.id;
      params["is_error"] = resp_ev->response.is_error ? 
	"true":"false";

      // decode result for easy use from script
      varPrintArg(resp_ev->response.data, params, resp_ev->response.is_error ? "error": "result");

      // decode udata for easy use from script
      varPrintArg(resp_ev->udata, params, "udata");

      // save reference to full parameters
      avar[DSM_AVAR_JSONRPCRESPONSEDATA] = AmArg(&resp_ev->response.data);
      avar[DSM_AVAR_JSONRPCRESPONSEUDATA] = AmArg(&resp_ev->udata);

      engine.runEvent(&dummy_session, this, DSMCondition::JsonRpcResponse, &params);

      avar.erase(DSM_AVAR_JSONRPCRESPONSEUDATA);
      avar.erase(DSM_AVAR_JSONRPCRESPONSEDATA);
      return;
    }

    JsonRpcRequestEvent* req_ev = 
      dynamic_cast<JsonRpcRequestEvent*>(jsonrpc_ev);
    if (req_ev) {
      map<string, string> params;
      params["ev_type"] = "JsonRpcRequest";
      params["is_notify"] = req_ev->isNotification() ? 
	"true" : "false";
      params["method"] = req_ev->method;
      if (!req_ev->id.empty())
	params["id"] = req_ev->id;

      // decode request params result for easy use from script
      varPrintArg(req_ev->params, params, "params");

      // save reference to full parameters
      avar[DSM_AVAR_JSONRPCREQUESTDATA] = AmArg(&req_ev->params);

      engine.runEvent(&dummy_session, this, DSMCondition::JsonRpcRequest, &params);

      avar.erase(DSM_AVAR_JSONRPCREQUESTDATA);
      return;
    }

  }

  if (event->event_id == E_SYSTEM) {
    AmSystemEvent* sys_ev = dynamic_cast<AmSystemEvent*>(event);
    if(sys_ev){	
      DBG("SystemDSM received system Event\n");      
      map<string, string> params;
      params["type"] = AmSystemEvent::getDescription(sys_ev->sys_event);
      engine.runEvent(&dummy_session, this, DSMCondition::System, &params);
      // stop_requested.set(true);
      return;
    }
  }

}

#define NOT_IMPLEMENTED(_func)					\
void SystemDSM::_func {						\
  throw DSMException("core", "cause", "not implemented");	\
}

#define NOT_IMPLEMENTED_UINT(_func)					\
  unsigned int SystemDSM::_func {					\
    throw DSMException("core", "cause", "not implemented");		\
  }

NOT_IMPLEMENTED(playPrompt(const string& name, bool loop));
NOT_IMPLEMENTED(playFile(const string& name, bool loop, bool front));
NOT_IMPLEMENTED(recordFile(const string& name));
NOT_IMPLEMENTED_UINT(getRecordLength());
NOT_IMPLEMENTED_UINT(getRecordDataSize());
NOT_IMPLEMENTED(stopRecord());
NOT_IMPLEMENTED(setInOutPlaylist());
NOT_IMPLEMENTED(setInputPlaylist());
NOT_IMPLEMENTED(setOutputPlaylist());

NOT_IMPLEMENTED(addToPlaylist(AmPlaylistItem* item));
NOT_IMPLEMENTED(closePlaylist(bool notify));
NOT_IMPLEMENTED(setPromptSet(const string& name));
NOT_IMPLEMENTED(addSeparator(const string& name, bool front));
NOT_IMPLEMENTED(connectMedia());
NOT_IMPLEMENTED(disconnectMedia());
NOT_IMPLEMENTED(mute());
NOT_IMPLEMENTED(unmute());

/** B2BUA functions */
NOT_IMPLEMENTED(B2BconnectCallee(const string& remote_party,
				 const string& remote_uri,
				 bool relayed_invite));
NOT_IMPLEMENTED(B2BterminateOtherLeg());
NOT_IMPLEMENTED(B2BaddReceivedRequest(const AmSipRequest& req));
NOT_IMPLEMENTED(B2BsetHeaders(const string& hdr, bool replaceCRLF));
NOT_IMPLEMENTED(B2BclearHeaders());
NOT_IMPLEMENTED(B2BaddHeader(const string& hdr));
NOT_IMPLEMENTED(transferOwnership(DSMDisposable* d));
NOT_IMPLEMENTED(releaseOwnership(DSMDisposable* d));

#undef NOT_IMPLEMENTED
#undef NOT_IMPLEMENTED_UINT
