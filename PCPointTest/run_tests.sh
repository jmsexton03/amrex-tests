srun --cpu-bind=map_cpu:0 --cpus-per-task=1 -n1 -N1 ./main3d.gnu.x86-milan.TPROF.MPI.ex inputs_s1 steps=100 | tee out.01.inv

srun --cpu-bind=map_cpu:0,1,2,3 --cpus-per-task=1 -n4 -N1 ./main3d.gnu.x86-milan.TPROF.MPI.ex inputs_s4 steps=100 | tee out.04.inv

srun --cpu-bind=map_cpu:0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 --cpus-per-task=1 -n16 -N1 ./main3d.gnu.x86-milan.TPROF.MPI.ex inputs_s16 steps=100 | tee out.16.inv

srun --cpu-bind=map_cpu:0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63 --cpus-per-task=1 -n64 -N1 ./main3d.gnu.x86-milan.TPROF.MPI.ex inputs_s64 steps=100 | tee out.64.inv
