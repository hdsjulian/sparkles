pause = 1000 # 1 second
speed = 100 # 0.1 second
spread_time = 100
reps = 5 # 5 repetitions
anireps = 20
num_devices = 10
base_time = (pause+speed)*(reps+1)*(anireps+1)
print("Base time: ", base_time)
for i in range(anireps):
    for j in range(reps+1):
        if (j <= reps/2):
            base_time += (spread_time/(anireps/2))*num_devices*j
        else:
            base_time += (spread_time/(anireps/2))*num_devices*(reps-j)
print(f"Total time: {base_time} ms")
