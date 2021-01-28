clc; close all; 
clear all;
%
load('test_audio.txt')


dataset = test_audio;
t = dataset(:,1) - dataset(1,1);
y_start = dataset(:,17);
y_pause = dataset(:,18);
y_grasp = dataset(:,19);

figure; 
plot(t, y_start); 
hold on;
plot(t, y_pause);
plot(t, y_grasp);
legend('start','stop','grasp')
