#!/usr/bin/env python3

import kivy
from kivy.app import App
from kivy.uix.gridlayout import GridLayout
from kivy.lang import Builder
from kivy.uix.slider import Slider
from kivy.uix.label import Label
from kivy.uix.screenmanager import ScreenManager, Screen
import paramiko, threading
import time, os, subprocess
from subprocess import Popen

from kivy.config import Config
Config.set('graphics', 'width', '1400')
Config.set('graphics', 'height', '900')


class ssh:
    shell = None
    client = None
    transport = None

    def __init__(self, address, username, password):
        print("Connecting to server on ip", str(address) + ".")
        self.client = paramiko.client.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.client.AutoAddPolicy())
        self.client.connect(address, username=username, password=password, look_for_keys=False)
        self.transport = paramiko.Transport((address, 22))
        self.transport.connect(username=username, password=password)

        # thread = threading.Thread(target=self.process)
        # thread.daemon = True
        # thread.start()

    def closeConnection(self):
        if(self.client != None):
            self.client.close()
            self.transport.close()

    def openShell(self):
        self.shell = self.client.invoke_shell()

    def sendShell(self, command):
        if(self.shell):
            self.shell.send(command + "\n")
        else:
            print("Shell not opened.")

    def process(self):
        global connection
        while True:
            # Print data when available
            if self.shell != None and self.shell.recv_ready():
                alldata = self.shell.recv(1024)
                while self.shell.recv_ready():
                    alldata += self.shell.recv(1024)
                strdata = str(alldata, "utf8")
                strdata.replace('\r', '')
                print(strdata, end = "")
                if(strdata.endswith("$ ")):
                    print("\n$ ", end = "")


## Kivy GUI code. Embedded in the python with the Builder class.
Builder.load_string("""
#:kivy 1.9.1
<CustButton@Button>:
    font_size: 32

<ScreenOne>:
    GridLayout:
        id: start
        rows: 1
        padding: 100
        spacing: 100

        BoxLayout:
            Button:
                font_size: 42
                text: "Press to initate Demo setup"
                on_press:
                    root.say_hello()
                    root.manager.transition.direction = 'left'
                    root.manager.transition.duration = 1
                    root.manager.current = 'screen_two'

<ScreenTwo>:
    GridLayout:
        id: mainLayout
        rows: 5
        padding: 50
        spacing: 10

        BoxLayout:
            spacing: 10
            CustButton:
                text: "Start Audio  (No interference)"
                on_press: 
                    root.start_ssh()
                    root.interference_setup()
                    root.manager.transition.direction = 'left'
                    root.manager.transition.duration = 1
                    root.manager.current = 'screen_three'


        BoxLayout:
            spacing: 10
            CustButton:
                text: "Start Audio (Interference)"
                on_press:
                    root.start_ssh_singleMode()
                    root.add_interference()
                    root.manager.transition.direction = 'left'
                    root.manager.transition.duration = 1
                    root.manager.current = 'screen_four'

        BoxLayout:
            spacing: 10
            CustButton:
                text: "Start Audio (Interference and Adaptive Codec)"
                on_press:
                    root.start_ssh()
                    root.add_interference()
                    root.manager.transition.direction = 'left'
                    root.manager.transition.duration = 1
                    root.manager.current = 'screen_five'

        BoxLayout:
            orientation: 'vertical'
            Label:
                text: str(slider.value)
            Slider:
                id: slider
                min: 0
                max: 100
                step: 1
                value: 0
                on_touch_up: root.slider_up(slider.value)


<ScreenThree>:
    GridLayout:
        rows: 5
        padding: 100
        spacing: 10
        
        BoxLayout:
            Label:
                font_size: 36    
                text: "This is the original audio stream. No interference and non-adaptive codec"

        BoxLayout:
            Button:
                font_size: 36
                text: "Go back"
                on_press:
                    root.manager.transition.direction = 'right'
                    root.manager.current = 'screen_two'
                    root.clear()

<ScreenFour>:
    GridLayout:
        rows: 5
        padding: 100
        spacing: 10
        
        BoxLayout:
            Label:
                font_size: 36    
                text: "Audio with interference and non-adaptive codec"

        BoxLayout:
            Button:
                font_size: 36
                text: "Go back"
                on_press:
                    root.manager.transition.direction = 'right'
                    root.manager.current = 'screen_two'
                    root.clear_singleMode()
                    root.interference_setup()

<ScreenFive>:
    GridLayout:
        rows: 5
        padding: 100
        spacing: 10
        
        BoxLayout:
            Label:
                font_size: 36    
                text: "Audio with interference and adaptive codec"

        BoxLayout:
            Button:
                font_size: 36
                text: "Go back"
                on_press:
                    root.manager.transition.direction = 'right'
                    root.manager.current = 'screen_two'
                    root.clear()
                    root.interference_setup()

""")

class ScreenOne(Screen):
    def __init__(self, **kwargs):
        super(ScreenOne, self).__init__(**kwargs)

    def say_hello(self):
        print("hello")


class ScreenTwo(Screen):
    global connection1, connection2
    serverUsername = "qoe"
    serverPassword = "D@RP@!!"
    serverServer = "192.168.100.2"
    # serverServer = "10.105.220.95"
    connection1 = ssh(serverServer, serverUsername, serverPassword)

    clientUsername = "qoe"
    clientPassword = "D@RP@!!"
    clientServer = "192.168.100.4"
    # clientServer = "10.105.216.187"
    connection2 = ssh(clientServer, clientUsername, clientPassword)

    command = 'tc qdisc add dev enp0s20f0u4 root netem loss 0% 0%'.split()
    command2 = 'tc qdisc change dev enp0s20f0u4 root netem loss 0% 0%'.split()
    cmd1 = subprocess.Popen(['echo','D@RP@!!'], stdout=subprocess.PIPE)
    cmd2 = subprocess.Popen(['sudo','-S'] + command, stdin=cmd1.stdout, stdout=subprocess.PIPE)
    cmd3 = subprocess.Popen(['sudo','-S'] + command2, stdin=cmd1.stdout, stdout=subprocess.PIPE)
    #subprocess.Popen('sudo tc qdisc add dev enp0s20f0u4 root netem loss 0%', shell=True, stdout=subprocess.PIPE) 

    def start_ssh(self):
        print("hello")
        #threads = []

        #t = threading.Thread(target=server_setup,)
        #t.start()
        #threads.append(t)

        connection1.openShell()
        connection1.sendShell("cd Documents/DARPA_QOE-master/DARPA_QOE_Server/Code\n")
        connection1.sendShell("./s2g\n")
        time.sleep(.2)

        #t = threading.Thread(target=client_setup,)
        #t.start()
        #threads.append(t)

        connection2.openShell()
        connection2.sendShell("cd Documents/DARPA_QOE-master/DARPA_QOE_Client/Code\n")
        connection2.sendShell("./c2\n")


        #for t in threads:
        #    t.join()

    def start_ssh_singleMode(self):
        print("hello")
        #threads = []

        #t = threading.Thread(target=server_setup,)
        #t.start()
        #threads.append(t)

        connection1.openShell()
        connection1.sendShell("cd Documents/DARPA_QOE-master/DARPA_QOE_Server/Code\n")
        connection1.sendShell("./s2g\n")
        time.sleep(.2)

        #t = threading.Thread(target=client_setup,)
        #t.start()
        #threads.append(t)

        connection2.openShell()
        connection2.sendShell("cd Documents/DARPA_QOE-master/DARPA_QOE_Client/Code\n")
        connection2.sendShell("./c2_mode1\n")


        #for t in threads:
        #    t.join()

    def clear(self):
        connection1.openShell()
        connection1.sendShell("killall -9 s2g\n")
        #connection1.closeConnection()

        connection2.openShell()
        connection2.sendShell("killall -9 c2\n")
        #connection2.closeConnection()

    # def on_touch_up(self, touch):
    #     released = super(ScreenTwo, self).on_touch_up(touch)
    #     if released:
    #         string = os.system('ls -l')
    #         print('string')
    #     return released

    def slider_up(self, value):
            sliderValue = int(value)
            string = os.system('ls -l')
            print(sliderValue)
            processes = [Popen("sudo tc qdisc change dev enp0s20f0u4 root netem loss {sliderValue}%".format(sliderValue=sliderValue), shell=True)]

            exitcodes = [p.wait() for p in processes]

    def interference_setup(self):
            command = 'tc qdisc add dev enp0s20f0u4 root netem loss 0% 0%'.split()
            command2 = 'tc qdisc change dev enp0s20f0u4 root netem loss 0% 0%'.split()
            cmd1 = subprocess.Popen(['echo','D@RP@!!'], stdout=subprocess.PIPE)
            cmd2 = subprocess.Popen(['sudo','-S'] + command, stdin=cmd1.stdout, stdout=subprocess.PIPE)
            cmd3 = subprocess.Popen(['sudo','-S'] + command2, stdin=cmd1.stdout, stdout=subprocess.PIPE)

    def add_interference(self):
            command = 'tc qdisc change dev enp0s20f0u4 root netem loss 14% 0%'.split()
            cmd1 = subprocess.Popen(['echo','D@RP@!!'], stdout=subprocess.PIPE)
            cmd2 = subprocess.Popen(['sudo','-S'] + command, stdin=cmd1.stdout, stdout=subprocess.PIPE)
            #processes = [Popen("sudo tc qdisc change dev enp0s20f0u4 root netem loss 11% 5%", shell=True)]
            #exitcodes = [p.wait() for p in processes]

    # Need to override the on_touch_up method to catch only one event.
    def on_touch_up(self, touch):
        # here, you don't check if the touch collides or things like that.
        # you just need to check if it's a grabbed touch event
        if touch.grab_current is self:

            # ok, the current touch is dispatched for us.
            # do something interesting here
            print('Hello world!')

            # don't forget to ungrab ourself, or you might have side effects
            touch.ungrab(self)

            # and accept the last up
            return True


class ScreenThree(Screen):
    def clear(self):
        connection1.openShell()
        connection1.sendShell("killall -9 s2g\n")
        #connection1.closeConnection()

        connection2.openShell()
        connection2.sendShell("killall -9 c2\n")
        #connection2.closeConnection()

class ScreenFour(Screen):
    def clear_singleMode(self):
        connection1.openShell()
        connection1.sendShell("killall -9 s2g\n")
        #connection1.closeConnection()

        connection2.openShell()
        connection2.sendShell("killall -9 c2_mode1\n")
        #connection2.closeConnection()

    def interference_setup(self):
            command = 'tc qdisc add dev enp0s20f0u4 root netem loss 0% 0%'.split()
            command2 = 'tc qdisc change dev enp0s20f0u4 root netem loss 0% 0%'.split()
            cmd1 = subprocess.Popen(['echo','D@RP@!!'], stdout=subprocess.PIPE)
            cmd2 = subprocess.Popen(['sudo','-S'] + command, stdin=cmd1.stdout, stdout=subprocess.PIPE)
            cmd3 = subprocess.Popen(['sudo','-S'] + command2, stdin=cmd1.stdout, stdout=subprocess.PIPE)

class ScreenFive(Screen):
    def clear(self):
        connection1.openShell()
        connection1.sendShell("killall -9 s2g\n")
        #connection1.closeConnection()

        connection2.openShell()
        connection2.sendShell("killall -9 c2\n")
        #connection2.closeConnection()

    def interference_setup(self):
            command = 'tc qdisc add dev enp0s20f0u4 root netem loss 0% 0%'.split()
            command2 = 'tc qdisc change dev enp0s20f0u4 root netem loss 0% 0%'.split()
            cmd1 = subprocess.Popen(['echo','D@RP@!!'], stdout=subprocess.PIPE)
            cmd2 = subprocess.Popen(['sudo','-S'] + command, stdin=cmd1.stdout, stdout=subprocess.PIPE)
            cmd3 = subprocess.Popen(['sudo','-S'] + command2, stdin=cmd1.stdout, stdout=subprocess.PIPE)

# def server_setup():
#     serverUsername = "jparks"
#     serverPassword = "root"
#     serverServer = "10.0.0.209"
#     #command = "ls"
#     #print("hello")
#     connection1 = ssh(serverServer, serverUsername, serverPassword)
#     connection1.openShell()
#     connection1.sendShell("cd Downloads\n")
#     connection1.sendShell("./s2\n")
#
#
# def client_setup():
#     clientUsername = "jparks"
#     clientPassword = "root"
#     clientServer = "10.0.0.149"
#     #command = "ls"
#     #print("hello")
#     connection2 = ssh(clientServer, clientUsername, clientPassword)
#     connection2.openShell()
#     connection2.sendShell("cd Downloads\n")
#     connection2.sendShell("./c2\n")
#
#
# def server_teardown():
#     connection1.sendShell("killall -9 s2")
#     # serverUsername = "jparks"
#     # serverPassword = "root"
#     # serverServer = "10.0.0.209"
#     # #command = "ls"
#     # #print("hello")
#     # connection1 = ssh(serverServer, serverUsername, serverPassword)
#     # connection1.openShell()
#     # connection1.sendShell("cd Downloads\n")
#     # connection1.sendShell("./s2\n")
#
# def client_teardown():
#     connection2.sendShell("killall -9 c2")
#     # clientUsername = "jparks"
#     # clientPassword = "root"
#     # clientServer = "10.0.0.149"
#     # #command = "ls"
#     # #print("hello")
#     # connection2 = ssh(clientServer, clientUsername, clientPassword)
#     # connection2.openShell()
#     # connection2.sendShell("cd Downloads\n")
#     # connection2.sendShell("./c2\n")

screen_manager = ScreenManager()

screen_manager.add_widget(ScreenOne(name="screen_one"))
screen_manager.add_widget(ScreenTwo(name="screen_two"))
screen_manager.add_widget(ScreenThree(name="screen_three"))
screen_manager.add_widget(ScreenFour(name="screen_four"))
screen_manager.add_widget(ScreenFive(name="screen_five"))


class App(App):
    def build(self):
        return screen_manager


if __name__ == '__main__':
    App().run()
