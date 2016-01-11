function cv
{
	if	[[ $PWD == */arch/$HOSTTYPE?(/*) ]]
	then	cd ${PWD/"arch/$HOSTTYPE"}&&pwd
	else	cd -
	fi
}
