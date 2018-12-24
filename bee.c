#include <libwa/wa.h>
#include <libwa/session.h>
#include <bitlbee.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct im_connection im_connection_t;

typedef struct {
	wa_t *wa;
	im_connection_t *ic;
	pthread_t thread;
	int pipe[2];
	GAsyncQueue *q;
} wabee_t;

enum event {
	EVENT_CONNECTED,
	EVENT_PRIV_MSG,
	EVENT_UPDATE_USER,
};

typedef struct {
	enum event type;
	void *data;
} event_t;

static int
event_add(wabee_t *wb, enum event type, void *data);


static int
cb_priv_msg(void *ptr, priv_msg_t *pm)
{
	return event_add(ptr, EVENT_PRIV_MSG, pm);
}

static int
cb_update_user(void *ptr, user_t *u)
{
	return event_add(ptr, EVENT_UPDATE_USER, u);
}

static int
handle_priv_msg(wabee_t *wb, priv_msg_t *pm)
{
	assert(pm->from);
	assert(pm->from->jid);
	assert(pm->text);

	imcb_buddy_msg(wb->ic, pm->from->jid, pm->text, 0, 0);

	g_free(pm);

	return 0;
}

static int
handle_update_user(wabee_t *wb, user_t *u)
{
	/* FIXME: Don't free the user u! */

	assert(u);

	fprintf(stderr, "Updating user %s (%s)\n", u->name, u->jid);

	// Search first
	bee_user_t *bu = bee_user_by_handle(wb->ic->bee, wb->ic, u->jid);

	if(!bu)
	{
		// User is new, create bee_user_t
		imcb_add_buddy(wb->ic, u->jid, NULL);
	}

	if(u->name)
		imcb_rename_buddy(wb->ic, u->jid, u->name);
	if(u->notify)
		imcb_buddy_nick_hint(wb->ic, u->jid, u->notify);

	imcb_buddy_status(wb->ic, u->jid, OPT_LOGGED_IN | OPT_AWAY, NULL,
		NULL);
	return 0;
}

static int
event_add(wabee_t *wb, enum event type, void *data)
{
	event_t *ev;

	ev = g_new0(event_t, 1);

	ev->type = type;
	ev->data = data;

	g_async_queue_push(wb->q, ev); /* No return errors? */


	return 0;
}

static int
wabee_send_priv_msg(struct im_connection *ic, char *who, char *text,
	int __attribute__((unused)) away)
{
	wabee_t *wb = ic->proto_data;
	return wa_send_priv_msg(wb->wa, who, text);
}

static gboolean
queue_dispatch(gpointer ptr, gint __attribute__((unused)) fd,
		     b_input_condition __attribute__((unused)) cond)
{
	wabee_t *wb = ptr;
	event_t *ev;

	//fprintf(stderr, "Dispatching events\n");

	while((ev = g_async_queue_try_pop(wb->q)))
	{

		switch(ev->type)
		{
			case EVENT_PRIV_MSG:
				handle_priv_msg(wb, ev->data);
				break;
			case EVENT_UPDATE_USER:
				handle_update_user(wb, ev->data);
				break;
			case EVENT_CONNECTED:
				imcb_connected(wb->ic);
				break;
			default:
				fprintf(stderr, "Unknown event type %d\n", ev->type);
		}

		g_free(ev);
	}

	return 1;
}

static wabee_t *
wabee_new(account_t *a)
{
	wabee_t *wb = g_new0(wabee_t, 1);
	cb_t *cb = g_new0(cb_t, 1);

	cb->ptr = wb;
	cb->priv_msg = cb_priv_msg;
	cb->update_user = cb_update_user;

	wb->wa = wa_init(cb);
	wb->ic = imcb_new(a);

	wb->ic->proto_data = wb;

	wb->q = g_async_queue_new();

	b_timeout_add(50, queue_dispatch, wb);

	return wb;
}

static void *
wabee_loop(void *ptr)
{
	wabee_t *wb = (wabee_t *) ptr;
	wa_login(wb->wa);

	event_add(wb, EVENT_CONNECTED, NULL);

	/*imcb_connected(wb->ic);*/
	wa_loop(wb->wa);

	return NULL;
}


static int
wabee_run(wabee_t *wb)
{
	if(pthread_create(&wb->thread, NULL, wabee_loop, wb))
	{
		fprintf(stderr, "error creating thread\n");
		return -1;
	}

	return 0;
}

void
wabee_login(account_t *a)
{
	/* This should run in his own thread. */
	wabee_t *wb = wabee_new(a);
	imcb_log(wb->ic, "Login started, check for QR in bitlbee stdout");

	wabee_run(wb);
}

#ifdef BITLBEE_ABI_VERSION_CODE
struct plugin_info *
init_plugin_info(void)
{
	static struct plugin_info info = {
	    BITLBEE_ABI_VERSION_CODE,
	    "bitlbe-wa",
	    "0.0.1",
	    "Bitlbee plugin for WhatsApp",
	    "Rodrigo Arias <rodarima@gmail.com>",
	    "https://github.com/rodarima/libwa"};

	return &info;
}
#endif

G_MODULE_EXPORT void init_plugin(void)
{

	struct prpl *ret = g_new0(struct prpl, 1);

	ret->options = PRPL_OPT_NOOTR | PRPL_OPT_NO_PASSWORD;
	ret->name = "whatsapp";
	ret->login = wabee_login;
	ret->buddy_msg = wabee_send_priv_msg;
//	ret->get_info = wa_get_info;
//	ret->add_buddy = wa_add_buddy;
//	ret->remove_buddy = wa_remove_buddy;
//	ret->chat_msg = wa_chat_msg;
//	ret->chat_join = wa_chat_join;
//	ret->chat_leave = wa_chat_leave;
//	ret->add_permit = wa_add_permit;
//	ret->rem_permit = wa_rem_permit;
//	ret->add_deny = wa_add_deny;
//	ret->rem_deny = wa_rem_deny;
//	ret->buddy_data_add = wa_buddy_data_add;
//	ret->buddy_data_free = wa_buddy_data_free;
	ret->handle_cmp = g_ascii_strcasecmp;

	register_protocol(ret);
}

