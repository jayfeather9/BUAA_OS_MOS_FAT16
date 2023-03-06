mkdir mydir
chmod a+rwx mydir
touch myfile
echo 2023 > myfile
mv moveme mydir/
cp copyme mydir/copied
cat readme
gcc bad.c 2>err.txt
if [ $# -eq 1 ] 
then
	n=$1
else
	n=10
fi
mkdir gen
a=1
while [ $a -le $n ]
do
	touch gen/$a.txt
	let a=a+1
done
