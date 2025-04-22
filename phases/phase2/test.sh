for ((i=0; i<47; i++)); do
    if [[ $i -lt 10 ]]; then
        exname="test0$i"
    else
        exname="test$i"
    fi
    echo "Running test $exname"
    ./$exname > out
    outname="testcases/$exname.out"
    diff out $outname
done
