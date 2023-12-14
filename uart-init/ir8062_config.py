class SerialConfig:
    def __init__(self):
        self.com_port=None
        self.transfer_port='RS485'
        self.x=None
        self.y=None
        self.w=None
        self.h=None
        self.alarm=None
        self.over_temperature=None
        self.under_temperature=None
    def set(self,x,y,w,h,alarm,OverTemperature,UnderTemperature):
        self.x=x
        self.y=y
        self.w=w
        self.h=h
        self.alarm=alarm
        self.over_temperature=OverTemperature
        self.under_temperature=UnderTemperature
    def set_serial(self,port):
        self.com_port=port
    def set_x(self,x):
        self.x=x
    def set_y(self,y):
        self.y=y
    def set_w(self,w):
        self.w=w
    def set_h(self,h):
        self.h=h
    def set_alarm(self,alarm):
        self.alarm=alarm
    def set_over_temperature(self,OverTemperature) :
        self.over_temperature=OverTemperature
    def set_under_temperature(self,UnderTemperature):
        self.under_temperature=UnderTemperature
    def display_info(self):
        print(f"x={self.x}, y={self.y}, w={self.w}, h={self.h}, alarm={self.alarm}, Over={self.OverTemperature}, Under={self.UnderTemperature}, ")

class EthernetConfig:
    def __init__(self):
        self.com_port=None
        self.transfer_port='RJ45'
        self.transfer_format=None
        self.alarm=None
        self.over_temperature=None
        self.under_temperature=None
        self.alert_temperature=None
    def set_serial(self,port):
        self.com_port=port
    def set_transfer_format(self,format):
        self.transfer_format=format
    def set_alarm(self,alarm):
        self.alarm=alarm
    def set_over_temperature(self,over):
        self.over_temperature=over
    def set_under_temperature(self,under):
        self.under_temperature=under
    def set_alert_temperature(self,alert):
        self.alert_temperature=alert
