/*
=============
Vote

	Handles a player's yes/no vote input, validating and applying the choice.
	=============
	*/
	inline void Vote(gentity_t* ent, const CommandArgs& args) {
	if (!level.vote.time) {
	gi.Client_Print(ent, PRINT_HIGH, "No vote in progress.\n");
	return;
	}
	if (ent->client->pers.voted != 0) {
		gi.Client_Print(ent, PRINT_HIGH, "You have already voted.\n");
		return;
	}
	if (args.count() < 2) {
	PrintUsage(ent, args, "<yes|no>", "", "Casts your vote.");
	return;
	}

	std::string asciiError;
	if (!ValidatePrintableASCII(args.getString(1), "Vote choice", asciiError)) {
	asciiError.push_back('\n');
	gi.Client_Print(ent, PRINT_HIGH, asciiError.c_str());
	PrintUsage(ent, args, "<yes|no>", "", "Casts your vote.");
	return;
	}

	// Accept "1"/"0" as well for convenience.
	std::string arg = (args.count() > 1) ? std::string(args.getString(1)) : "";
	std::ranges::transform(arg, arg.begin(), [](char ch) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	});
	if (arg == "1") arg = "yes";
	if (arg == "0") arg = "no";
	if (arg != "yes" && arg != "y" && arg != "no" && arg != "n") {
		PrintUsage(ent, args, "<yes|no>", "", "Cast your vote.");
		return;
	}

	std::string_view vote = arg;
	if (vote == "yes" || vote == "y") {
		level.vote.countYes++;
		ent->client->pers.voted = 1;
	}
	else if (vote == "no" || vote == "n") {
		level.vote.countNo++;
		ent->client->pers.voted = -1;
	}
	else {
		PrintUsage(ent, args, "<yes|no>", "", "Casts your vote.");
		return;
	}

	gi.Client_Print(ent, PRINT_HIGH, "Vote cast.\n");
	}
