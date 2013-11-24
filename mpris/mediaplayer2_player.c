
#include "mediaplayer2_player.h"
#include "message.h"

int mediaplayer2_player_playpause_eval(DBusConnection *dbus, DBusMessage *msg,
				       const struct remote_callback_ops *cb_ops,
				       const void *cb_data)
{
	cb_ops->toggle_playback(cb_data);
	mpris_message_send_empty_return(dbus, msg);
	return 0;
}
