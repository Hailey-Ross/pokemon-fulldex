// NOW PLAYING (frontend/now_playing.h): Android shows other apps' music sessions only to an app the
// player has given notification access, in Settings. This empty listener is what that access is
// given to; it reads no notifications. NowPlaying asks for the music sessions in its name.
package io.github.mamesj.pokemonds;

import android.service.notification.NotificationListenerService;

public class NowPlayingListener extends NotificationListenerService {
}
