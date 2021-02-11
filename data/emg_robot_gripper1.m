clc; close all; 
clear all;

load('trial4_02_02_2021.txt')
dataset = trial4_02_02_2021;

%%
t = dataset(:,1);
% variables saved - from robotic system
ee_posx = dataset(:,2);
ee_posy = dataset(:,3);
ee_posz = dataset(:,4);
target_posx = dataset(:,5);
target_posy = dataset(:,6);
target_posz = dataset(:,7);
ee_vel_norm = dataset(:,8);

predicted_label = dataset(:,9);
startRobotMotion = dataset(:,10);
pauseRobotMotion = dataset(:,11);

% gripper_pos = dataset(:,12); %between 0(open) and 255(closed)
% gripper_status = dataset(:,13); %if stopped opening/closing due t ocntact w object
% gripper_state = dataset(:,14); %0 open, 1 closed
activategripper = dataset(:,15); %if label 7->5
% desgripper_cmd = dataset(:,16); % 1 to close, 2 to open

%ground truth labels
y_start = dataset(:,17);
y_pause = dataset(:,18);
y_grasp = dataset(:,19);
%%
% get vector w times of ground truth and start motion first 1 received
% get number of activations
end_cuestart_idx = find(diff(y_pause) == -1);
t_cuestart = t(end_cuestart_idx);
emgstart_idx = find(diff(pauseRobotMotion) == 1);
t_emgstart = t(emgstart_idx);
tdelays_cueemg = t_emgstart - t_cuestart;
boxplot(tdelays_cueemg)
% ave delat between end of audio cue and first 1 detected
ave_tdelays_cueemg = mean(tdelays_cueemg);
std_tdelays_cueemg = std (tdelays_cueemg);

%%
% for CR < 100%, want a function to calculate delays




%%
figure; 
plot(t, y_pause); 
hold on;
plot(t, pauseRobotMotion)
legend('audiopause','pausemotion')

end_cuestart_idx = find(diff(y_start) == -1);
t_cuestart = t(end_cuestart_idx);
emgstart_idx = find(diff(startRobotMotion) == 1);
t_emgstart = t(emgstart_idx);
tdelays_cueemg = t_emgstart - t_cuestart;
boxplot(tdelays_cueemg)
% ave delat between end of audio cue and first 1 detected
ave_tdelays_cueemg = mean(tdelays_cueemg);
std_tdelays_cueemg = std (tdelays_cueemg);
%%

figure; 
plot(t, y_pause); 
hold on;
plot(t, pauseRobotMotion)
legend('audio','pausemotion')

%%


figure; 
plot(t, y_start); 
% hold on;
% plot(t, y_pause);
% plot(t, y_grasp);
% legend('start','stop','grasp')
