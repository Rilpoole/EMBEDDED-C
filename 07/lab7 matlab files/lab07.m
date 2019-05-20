clear all; close all; clc; 
load("RileyLab7.mat")

%time vector
time = [];
time(1) = 0;
for i = 1:length(Velocity)-1
    time(i+1) = Bti/1000*i; %time in seconds
    time = time'; 
end

%transfer function
s = tf('s');
cont = Kp*(s+Ki/Kp)/s;
Kvi = 0.41;
Kt = 0.11;
J = 3.8e-4;
B = 0; %assume very little damping
g1 = Kvi;
g2 = Kt; 
g3 = 1/(J*s+B);
sys_vel = feedback(cont * g1 * g2 * g3,1);
sys_tor = cont * g1 * g2 / (1+cont*g1*g2*g3); 

%compute theoretical velocity response
step_scale = (Current_Reference - Previous_Reference)*2*pi/60 ;
[theoretical_velocity,theoretical_velocity_time] = step(sys_vel);
theoretical_velocity = (theoretical_velocity * step_scale) + Velocity(1);

%compute theoretical torque response
step_scale = (Current_Reference - Previous_Reference)*2*pi/60 ;
[theoretical_torque,theoretical_torque_time] = step(sys_tor);
theoretical_torque = (theoretical_torque * step_scale);

subplot(2,1,1);
%plot the measured and theoretical torque
plot(time,Torque,'k.',theoretical_torque_time,theoretical_torque,'k');title("Measured and Theoretical Control Values");
xlabel("Time (s)"); ylabel("Control Value (N\cdotM)"); legend("Measured","Theoretical"); 

subplot(2,1,2); 
%plot the measured and theoretical velocitieis
plot(time,Velocity,'k.',theoretical_velocity_time,theoretical_velocity,'k');title("Measured and Theoretical Control Values");
xlabel("Time (s)"); ylabel("Velocity (rad/sec)"); legend("Measured","Theoretical"); 

