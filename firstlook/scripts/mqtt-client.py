import paho.mqtt.client as mqtt
import cbor2
import random

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("350976650591701/#")
    client.publish("350976650591701/my/subscribe/topic", payload=cbor2.dumps([[1, 1691405858, 100, 0, 100]]), qos=0, retain=False)
    

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    try:
        print(msg.topic + " " + str(cbor2.loads(msg.payload)))
    except:
        print(msg.topic + " " + str(msg.payload))
    if "publish" in msg.topic:
        client.publish("350976650591701/my/subscribe/topic",
                       payload=cbor2.dumps([
                           [1, 1691405858,
                            random.choice([0,50,100]),
                            random.choice([0,50,100]),
                            random.choice([0,50,100])]]),
                       qos=0, retain=False)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("test.mosquitto.org", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()