## How-to setup headless xwax with OSC

#### 1. Setting Up Real Time Priority

http://wiki.xwax.org/setting_up_real_time_audio_priority_for_xwax
http://www.jackaudio.org/faq/linux_rt_config.html

Set rtprio and memlock rights for your username to start real-time processes along with option to lock a big memory region in RAM.
This enables xwax "-k" parameter to lock all audio buffers in RAM and to never swap out WAVE data to disk.
xwax will consume around 1 GB memory maximum as it have ~50 minutes track length limit per deck.

```
echo "@realtime - rtprio 99" | sudo tee --append /etc/security/limits.d/99-realtime.conf
echo "@realtime - memlock unlimited" | sudo tee --append /etc/security/limits.d/99-realtime.conf
sudo groupadd realtime
RTUSER=`id -un`
sudo -E usermod -a -G realtime $RTUSER
```

#### 2. Install xwax dependencies

```
sudo apt-get build-dep xwax
sudo apt-get install mpg123 liblo-dev python-liblo
```

#### 3. Fetch xwax osc branch code

```
git clone https://github.com/oligau/xwax-headless.git
cd xwax-headless
git checkout osc
```

#### 4. Configure 

```
./configure --prefix /usr --enable-alsa --enable-osc
make
sudo make install
```

#### 5. Start xwax with OSC server and no GUI

We use -k to lock real-time memory into RAM. 

If you obtain error "Cannot allocate memory" or "Failed to get realtime priorities", you need to follow step 1 to configure real-time priority rights for your user, then logoff and logon to apply.


```
xwax -k -l ~/Music -a default --osc --headless
```

#### 6. Testing python client

##### Loading a track in deck 1

```
cd client
python client.py load 1 ~/Music/Test Artist\ -\ Test Track.wav
```

##### Starting playback on deck 1

```
python client.py set-pitch 1 --pitch 1.0
```

xwax should now be playing without any timecode or gui needed.

##### Stopping playback on deck 1

```
python client.py reconnect 1
```

Timecode is reconnected. It should stop playback if timecode is not currrently fed into input.
