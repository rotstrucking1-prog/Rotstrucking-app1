// AoCNPCSpeech.cpp
// Architect of Creation — NPC Contextual Speech Implementation
//
// Contains 300+ speech lines built directly into the binary as static data.
// Lines sound like real player chat — casual, with abbreviations, slang, and
// occasional typos. The system selects lines based on context, personality tone,
// and avoids recent repeats.

#include "AoCNPCCharacter.h"
#include "AoCNPCSpeech.h"
#include "AoCHumanoidNPCV2.h"
#include "AoCNPCBrainV2.h"
#include "AoCNPCRelationship.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPCSpeech, Log, All);

// ============================================================================
// CONSTRUCTOR / LIFECYCLE
// ============================================================================

UAoCNPCSpeech::UAoCNPCSpeech()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f; // 4Hz — check pending speech queue

	RecentlyUsedLines.SetNum(MaxRecentLines);
	for (int32& Idx : RecentlyUsedLines)
	{
		Idx = -1;
	}
}

void UAoCNPCSpeech::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AAoCNPCCharacter>(GetOwner());
	if (OwnerNPC)
	{
		RelationshipComp = OwnerNPC->FindComponentByClass<UAoCNPCRelationship>();
	}

	InitializeSpeechDatabase();

	UE_LOG(LogNPCSpeech, Log, TEXT("[%s] Speech system initialized with %d lines, TalkativeLevel=%.2f, Tone=%d"),
		OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
		SpeechDatabase.Num(), TalkativeLevel, (int32)DominantTone);
}

void UAoCNPCSpeech::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickPendingSpeech(DeltaTime);
}

// ============================================================================
// SPEECH DATABASE — 300+ LINES
// ============================================================================

void UAoCNPCSpeech::InitializeSpeechDatabase()
{
	SpeechDatabase.Empty();

	// Helper lambda to add lines compactly
	auto Add = [this](ESpeechContext Ctx, ESpeechTone Tone, const FString& Line)
	{
		FSpeechLine Entry;
		Entry.Context = Ctx;
		Entry.Tone = Tone;
		Entry.Line = Line;
		SpeechDatabase.Add(Entry);
	};

	// ================================================================
	// GREETINGS (20 lines)
	// ================================================================
	Add(ESpeechContext::GreetStranger, ESpeechTone::Friendly, TEXT("hey"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Friendly, TEXT("sup"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Friendly, TEXT("yo"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Polite,   TEXT("hey there"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Grumpy,   TEXT("what"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Grumpy,   TEXT("yeah?"));
	Add(ESpeechContext::GreetStranger, ESpeechTone::Any,      TEXT("hi"));

	Add(ESpeechContext::GreetFriend, ESpeechTone::Friendly, TEXT("hey! long time no see"));
	Add(ESpeechContext::GreetFriend, ESpeechTone::Friendly, TEXT("ayyy whats up"));
	Add(ESpeechContext::GreetFriend, ESpeechTone::Friendly, TEXT("there you are, was looking for you"));
	Add(ESpeechContext::GreetFriend, ESpeechTone::Polite,   TEXT("good to see you again"));
	Add(ESpeechContext::GreetFriend, ESpeechTone::Any,      TEXT("oh hey!"));

	Add(ESpeechContext::GreetEnemy, ESpeechTone::Aggressive, TEXT("oh great, you again"));
	Add(ESpeechContext::GreetEnemy, ESpeechTone::Aggressive, TEXT("what do you want"));
	Add(ESpeechContext::GreetEnemy, ESpeechTone::Aggressive, TEXT("stay away from me"));
	Add(ESpeechContext::GreetEnemy, ESpeechTone::Nervous,    TEXT("oh no"));
	Add(ESpeechContext::GreetEnemy, ESpeechTone::Nervous,    TEXT("not you again..."));

	Add(ESpeechContext::Farewell, ESpeechTone::Friendly, TEXT("later"));
	Add(ESpeechContext::Farewell, ESpeechTone::Friendly, TEXT("see ya"));
	Add(ESpeechContext::Farewell, ESpeechTone::Polite,   TEXT("take care"));

	// ================================================================
	// PARTY (30 lines)
	// ================================================================
	Add(ESpeechContext::PartyInviteAccept, ESpeechTone::Friendly, TEXT("sure lets go"));
	Add(ESpeechContext::PartyInviteAccept, ESpeechTone::Friendly, TEXT("yeah im down"));
	Add(ESpeechContext::PartyInviteAccept, ESpeechTone::Polite,   TEXT("sounds good, count me in"));
	Add(ESpeechContext::PartyInviteAccept, ESpeechTone::Any,      TEXT("alright"));
	Add(ESpeechContext::PartyInviteAccept, ESpeechTone::Any,      TEXT("sure why not"));

	Add(ESpeechContext::PartyInviteDecline, ESpeechTone::Polite,   TEXT("nah im good, thanks tho"));
	Add(ESpeechContext::PartyInviteDecline, ESpeechTone::Polite,   TEXT("sorry, kinda busy rn"));
	Add(ESpeechContext::PartyInviteDecline, ESpeechTone::Grumpy,   TEXT("no"));
	Add(ESpeechContext::PartyInviteDecline, ESpeechTone::Grumpy,   TEXT("nah"));
	Add(ESpeechContext::PartyInviteDecline, ESpeechTone::Any,      TEXT("not right now, maybe later"));

	Add(ESpeechContext::PartyInviteOffer, ESpeechTone::Friendly, TEXT("wanna group up?"));
	Add(ESpeechContext::PartyInviteOffer, ESpeechTone::Friendly, TEXT("hey want to party?"));
	Add(ESpeechContext::PartyInviteOffer, ESpeechTone::Polite,   TEXT("looking for a group if you're interested"));
	Add(ESpeechContext::PartyInviteOffer, ESpeechTone::Any,      TEXT("need one more for group?"));

	Add(ESpeechContext::PartyLootFair, ESpeechTone::Friendly, TEXT("nice, fair split"));
	Add(ESpeechContext::PartyLootFair, ESpeechTone::Any,      TEXT("good loot today"));
	Add(ESpeechContext::PartyLootFair, ESpeechTone::Polite,   TEXT("appreciate the fair share"));

	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Aggressive, TEXT("hey, that should've been mine"));
	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Aggressive, TEXT("you're taking everything, what about me?"));
	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Passive,    TEXT("um, I thought we were splitting things..."));
	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Passive,    TEXT("i don't wanna be that guy but i haven't gotten anything"));
	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Any,        TEXT("can i get some of that loot too"));
	Add(ESpeechContext::PartyLootUnfair, ESpeechTone::Grumpy,     TEXT("dude come on, share"));

	Add(ESpeechContext::PartyLootAngry, ESpeechTone::Aggressive, TEXT("that's it. we're done. you're a thief."));
	Add(ESpeechContext::PartyLootAngry, ESpeechTone::Aggressive, TEXT("i'm not your pack mule. give me my share or we have a problem"));
	Add(ESpeechContext::PartyLootAngry, ESpeechTone::Any,        TEXT("this is bs, you've taken everything"));
	Add(ESpeechContext::PartyLootAngry, ESpeechTone::Grumpy,     TEXT("forget this, im out"));

	Add(ESpeechContext::PartyBetray, ESpeechTone::Any,        TEXT("you brought this on yourself"));
	Add(ESpeechContext::PartyBetray, ESpeechTone::Any,        TEXT("should've shared the loot"));
	Add(ESpeechContext::PartyBetray, ESpeechTone::Aggressive, TEXT("my turn"));
	Add(ESpeechContext::PartyBetray, ESpeechTone::Aggressive, TEXT("nothing personal... ok maybe a little"));

	Add(ESpeechContext::PartyLeaving, ESpeechTone::Polite,   TEXT("gotta go, thanks for the group"));
	Add(ESpeechContext::PartyLeaving, ESpeechTone::Friendly, TEXT("peace, good session"));
	Add(ESpeechContext::PartyLeaving, ESpeechTone::Grumpy,   TEXT("im done"));
	Add(ESpeechContext::PartyLeaving, ESpeechTone::Any,      TEXT("heading out, gl"));

	Add(ESpeechContext::PartyEncourage, ESpeechTone::Friendly, TEXT("nice job team"));
	Add(ESpeechContext::PartyEncourage, ESpeechTone::Friendly, TEXT("we got this"));
	Add(ESpeechContext::PartyEncourage, ESpeechTone::Any,      TEXT("keep it up"));

	// ================================================================
	// COMBAT (45 lines)
	// ================================================================
	Add(ESpeechContext::CombatEngage, ESpeechTone::Aggressive, TEXT("you're dead"));
	Add(ESpeechContext::CombatEngage, ESpeechTone::Aggressive, TEXT("let's go"));
	Add(ESpeechContext::CombatEngage, ESpeechTone::Aggressive, TEXT("come here"));
	Add(ESpeechContext::CombatEngage, ESpeechTone::Any,        TEXT("here we go"));
	Add(ESpeechContext::CombatEngage, ESpeechTone::Nervous,    TEXT("i don't want to do this but you leave me no choice"));
	Add(ESpeechContext::CombatEngage, ESpeechTone::Nervous,    TEXT("ok ok lets do this i guess"));

	Add(ESpeechContext::CombatTaunt, ESpeechTone::Aggressive, TEXT("is that all you got?"));
	Add(ESpeechContext::CombatTaunt, ESpeechTone::Aggressive, TEXT("lmao"));
	Add(ESpeechContext::CombatTaunt, ESpeechTone::Aggressive, TEXT("too easy"));
	Add(ESpeechContext::CombatTaunt, ESpeechTone::Aggressive, TEXT("come on hit me harder"));
	Add(ESpeechContext::CombatTaunt, ESpeechTone::Sarcastic,  TEXT("oh scary"));
	Add(ESpeechContext::CombatTaunt, ESpeechTone::Sarcastic,  TEXT("lol ok"));

	Add(ESpeechContext::CombatLowHP, ESpeechTone::Any,     TEXT("i need a heal!"));
	Add(ESpeechContext::CombatLowHP, ESpeechTone::Any,     TEXT("i'm low!"));
	Add(ESpeechContext::CombatLowHP, ESpeechTone::Any,     TEXT("help!"));
	Add(ESpeechContext::CombatLowHP, ESpeechTone::Nervous, TEXT("oh no im dying"));
	Add(ESpeechContext::CombatLowHP, ESpeechTone::Nervous, TEXT("i'm so low omg"));

	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Aggressive, TEXT("gg"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Aggressive, TEXT("get rekt"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Aggressive, TEXT("sit down"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Aggressive, TEXT("ez"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Polite,     TEXT("good fight"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Polite,     TEXT("sorry about that"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Polite,     TEXT("gf"));
	Add(ESpeechContext::CombatKilledEnemy, ESpeechTone::Any,        TEXT("got em"));

	Add(ESpeechContext::CombatDying, ESpeechTone::Any,        TEXT("noooo"));
	Add(ESpeechContext::CombatDying, ESpeechTone::Any,        TEXT("bruh"));
	Add(ESpeechContext::CombatDying, ESpeechTone::Aggressive, TEXT("i'll be back for you"));
	Add(ESpeechContext::CombatDying, ESpeechTone::Nervous,    TEXT("please no"));

	Add(ESpeechContext::CombatCallForHelp, ESpeechTone::Any,     TEXT("help me!"));
	Add(ESpeechContext::CombatCallForHelp, ESpeechTone::Any,     TEXT("getting jumped over here"));
	Add(ESpeechContext::CombatCallForHelp, ESpeechTone::Nervous, TEXT("theres too many of them"));
	Add(ESpeechContext::CombatCallForHelp, ESpeechTone::Any,     TEXT("need backup"));

	Add(ESpeechContext::CombatWatchOut, ESpeechTone::Any,     TEXT("watch out!"));
	Add(ESpeechContext::CombatWatchOut, ESpeechTone::Any,     TEXT("behind you!"));
	Add(ESpeechContext::CombatWatchOut, ESpeechTone::Nervous, TEXT("look out!"));

	Add(ESpeechContext::CombatNiceKill, ESpeechTone::Any,      TEXT("nice!"));
	Add(ESpeechContext::CombatNiceKill, ESpeechTone::Any,      TEXT("clean kill"));
	Add(ESpeechContext::CombatNiceKill, ESpeechTone::Friendly, TEXT("nice one dude"));
	Add(ESpeechContext::CombatNiceKill, ESpeechTone::Any,      TEXT("damn that was clean"));

	Add(ESpeechContext::CombatRunAway, ESpeechTone::Any,     TEXT("nope nope nope"));
	Add(ESpeechContext::CombatRunAway, ESpeechTone::Any,     TEXT("im out"));
	Add(ESpeechContext::CombatRunAway, ESpeechTone::Nervous, TEXT("oh god gotta go"));
	Add(ESpeechContext::CombatRunAway, ESpeechTone::Any,     TEXT("running, don't judge me"));

	Add(ESpeechContext::CombatVictory, ESpeechTone::Aggressive, TEXT("lets gooo"));
	Add(ESpeechContext::CombatVictory, ESpeechTone::Friendly,   TEXT("nice we did it"));
	Add(ESpeechContext::CombatVictory, ESpeechTone::Any,        TEXT("gg"));
	Add(ESpeechContext::CombatVictory, ESpeechTone::Polite,     TEXT("well played everyone"));

	// ================================================================
	// GATHERING / CRAFTING (40 lines)
	// ================================================================
	Add(ESpeechContext::MiningFound, ESpeechTone::Any,      TEXT("nice, iron vein"));
	Add(ESpeechContext::MiningFound, ESpeechTone::Any,      TEXT("found some ore over here"));
	Add(ESpeechContext::MiningFound, ESpeechTone::Friendly, TEXT("ooh thats a good node"));
	Add(ESpeechContext::MiningFound, ESpeechTone::Any,      TEXT("jackpot"));
	Add(ESpeechContext::MiningFound, ESpeechTone::Any,      TEXT("sweet, more ore"));

	Add(ESpeechContext::MiningTired, ESpeechTone::Any,    TEXT("been mining forever, need a break"));
	Add(ESpeechContext::MiningTired, ESpeechTone::Any,    TEXT("my back is killing me lol"));
	Add(ESpeechContext::MiningTired, ESpeechTone::Grumpy, TEXT("this is so tedious"));
	Add(ESpeechContext::MiningTired, ESpeechTone::Any,    TEXT("how much more ore do i need..."));

	Add(ESpeechContext::MiningNodeEmpty, ESpeechTone::Any,    TEXT("empty, moving on"));
	Add(ESpeechContext::MiningNodeEmpty, ESpeechTone::Grumpy, TEXT("someone already mined this out"));
	Add(ESpeechContext::MiningNodeEmpty, ESpeechTone::Any,    TEXT("nothing here"));

	Add(ESpeechContext::WoodcuttingChop, ESpeechTone::Any,    TEXT("timber"));
	Add(ESpeechContext::WoodcuttingChop, ESpeechTone::Any,    TEXT("another one down"));
	Add(ESpeechContext::WoodcuttingChop, ESpeechTone::Grumpy, TEXT("my arms hurt"));

	Add(ESpeechContext::CraftingSuccess, ESpeechTone::Any,      TEXT("nice, turned out pretty good"));
	Add(ESpeechContext::CraftingSuccess, ESpeechTone::Any,      TEXT("done!"));
	Add(ESpeechContext::CraftingSuccess, ESpeechTone::Friendly, TEXT("finally finished crafting that"));
	Add(ESpeechContext::CraftingSuccess, ESpeechTone::Any,      TEXT("not bad not bad"));
	Add(ESpeechContext::CraftingSuccess, ESpeechTone::Any,      TEXT("ayy it worked"));

	Add(ESpeechContext::CraftingFailed, ESpeechTone::Any,        TEXT("ugh, failed"));
	Add(ESpeechContext::CraftingFailed, ESpeechTone::Any,        TEXT("are you kidding me"));
	Add(ESpeechContext::CraftingFailed, ESpeechTone::Grumpy,     TEXT("wasted all those mats for nothing"));
	Add(ESpeechContext::CraftingFailed, ESpeechTone::Any,        TEXT("wait no"));
	Add(ESpeechContext::CraftingFailed, ESpeechTone::Aggressive, TEXT("this game hates me"));

	Add(ESpeechContext::CraftingStarting, ESpeechTone::Any, TEXT("alright lets craft this"));
	Add(ESpeechContext::CraftingStarting, ESpeechTone::Any, TEXT("time to make something"));
	Add(ESpeechContext::CraftingStarting, ESpeechTone::Any, TEXT("gonna be here a while"));

	Add(ESpeechContext::FishingBite, ESpeechTone::Any,      TEXT("got one!"));
	Add(ESpeechContext::FishingBite, ESpeechTone::Friendly, TEXT("fish on!"));
	Add(ESpeechContext::FishingBite, ESpeechTone::Any,      TEXT("oh nice, something's biting"));

	Add(ESpeechContext::FishingNothing, ESpeechTone::Any,    TEXT("nothing biting today"));
	Add(ESpeechContext::FishingNothing, ESpeechTone::Any,    TEXT("this spot sucks"));
	Add(ESpeechContext::FishingNothing, ESpeechTone::Grumpy, TEXT("seriously? not a single bite"));
	Add(ESpeechContext::FishingNothing, ESpeechTone::Any,    TEXT("gonna try a different spot"));

	Add(ESpeechContext::FishingCaught, ESpeechTone::Any,      TEXT("nice catch"));
	Add(ESpeechContext::FishingCaught, ESpeechTone::Friendly, TEXT("haha that's a big one"));
	Add(ESpeechContext::FishingCaught, ESpeechTone::Any,      TEXT("finally"));

	Add(ESpeechContext::CookingDone, ESpeechTone::Any,      TEXT("food's ready"));
	Add(ESpeechContext::CookingDone, ESpeechTone::Friendly, TEXT("smells good"));
	Add(ESpeechContext::CookingDone, ESpeechTone::Any,      TEXT("dinner is served lol"));

	Add(ESpeechContext::SmeltingDone, ESpeechTone::Any, TEXT("bars done"));
	Add(ESpeechContext::SmeltingDone, ESpeechTone::Any, TEXT("smelting finished"));

	Add(ESpeechContext::GatheringHerb, ESpeechTone::Any,      TEXT("nice, rare herb"));
	Add(ESpeechContext::GatheringHerb, ESpeechTone::Any,      TEXT("got some herbs"));
	Add(ESpeechContext::GatheringHerb, ESpeechTone::Friendly, TEXT("ooh this one's good for potions"));

	// ================================================================
	// TRADING (18 lines)
	// ================================================================
	Add(ESpeechContext::TradeOffer, ESpeechTone::Friendly, TEXT("wanna trade?"));
	Add(ESpeechContext::TradeOffer, ESpeechTone::Polite,   TEXT("i've got some stuff if you're interested"));
	Add(ESpeechContext::TradeOffer, ESpeechTone::Any,      TEXT("selling iron bars"));
	Add(ESpeechContext::TradeOffer, ESpeechTone::Any,      TEXT("need anything?"));

	Add(ESpeechContext::TradeAccept, ESpeechTone::Any,      TEXT("deal"));
	Add(ESpeechContext::TradeAccept, ESpeechTone::Friendly, TEXT("sounds fair, deal"));
	Add(ESpeechContext::TradeAccept, ESpeechTone::Polite,   TEXT("sure, that works"));

	Add(ESpeechContext::TradeDecline, ESpeechTone::Polite,   TEXT("no thanks, not what im looking for"));
	Add(ESpeechContext::TradeDecline, ESpeechTone::Grumpy,   TEXT("nah that's a ripoff"));
	Add(ESpeechContext::TradeDecline, ESpeechTone::Any,      TEXT("ill pass"));
	Add(ESpeechContext::TradeDecline, ESpeechTone::Sarcastic, TEXT("lol no way"));

	Add(ESpeechContext::TradeHaggle, ESpeechTone::Any,      TEXT("can you go lower?"));
	Add(ESpeechContext::TradeHaggle, ESpeechTone::Friendly, TEXT("how about a better price for a friend?"));
	Add(ESpeechContext::TradeHaggle, ESpeechTone::Grumpy,   TEXT("that's way too much"));
	Add(ESpeechContext::TradeHaggle, ESpeechTone::Any,      TEXT("meet me halfway?"));

	Add(ESpeechContext::TradeThanks, ESpeechTone::Any,      TEXT("ty"));
	Add(ESpeechContext::TradeThanks, ESpeechTone::Polite,   TEXT("thanks for the trade"));
	Add(ESpeechContext::TradeThanks, ESpeechTone::Friendly, TEXT("pleasure doing business"));

	// ================================================================
	// SOCIAL (35 lines)
	// ================================================================
	Add(ESpeechContext::SmallTalk, ESpeechTone::Friendly, TEXT("nice day for it"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Friendly, TEXT("how's it going?"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Friendly, TEXT("anything good happening?"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Grumpy,   TEXT("what do you want"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Grumpy,   TEXT("leave me alone"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Any,      TEXT("hey"));
	Add(ESpeechContext::SmallTalk, ESpeechTone::Polite,   TEXT("hey, how are you?"));

	Add(ESpeechContext::Gossip, ESpeechTone::Any,      TEXT("heard there's good ore north of here"));
	Add(ESpeechContext::Gossip, ESpeechTone::Any,      TEXT("watch out for bandits on the east road"));
	Add(ESpeechContext::Gossip, ESpeechTone::Any,      TEXT("someone found mithril in the deep caves apparently"));
	Add(ESpeechContext::Gossip, ESpeechTone::Any,      TEXT("did you hear about the fight at the tavern last night"));
	Add(ESpeechContext::Gossip, ESpeechTone::Friendly, TEXT("apparently there's a group building a keep up north"));
	Add(ESpeechContext::Gossip, ESpeechTone::Any,      TEXT("prices for steel are way up rn"));

	Add(ESpeechContext::Warning, ESpeechTone::Any,     TEXT("careful, I got jumped near here earlier"));
	Add(ESpeechContext::Warning, ESpeechTone::Any,     TEXT("there's a group of players PKing south"));
	Add(ESpeechContext::Warning, ESpeechTone::Any,     TEXT("heads up, dangerous mobs ahead"));
	Add(ESpeechContext::Warning, ESpeechTone::Nervous, TEXT("i wouldn't go that way if i were you"));
	Add(ESpeechContext::Warning, ESpeechTone::Any,     TEXT("watch your back around here"));

	Add(ESpeechContext::Compliment, ESpeechTone::Friendly, TEXT("nice gear"));
	Add(ESpeechContext::Compliment, ESpeechTone::Friendly, TEXT("you're pretty good at this"));
	Add(ESpeechContext::Compliment, ESpeechTone::Polite,   TEXT("that was impressive"));
	Add(ESpeechContext::Compliment, ESpeechTone::Any,      TEXT("solid work"));

	Add(ESpeechContext::Insult, ESpeechTone::Aggressive, TEXT("you suck lol"));
	Add(ESpeechContext::Insult, ESpeechTone::Aggressive, TEXT("imagine being this bad"));
	Add(ESpeechContext::Insult, ESpeechTone::Sarcastic,  TEXT("oh wow, great job"));
	Add(ESpeechContext::Insult, ESpeechTone::Grumpy,     TEXT("get out of my way"));

	Add(ESpeechContext::AskForHelp, ESpeechTone::Any,      TEXT("anyone wanna help me out?"));
	Add(ESpeechContext::AskForHelp, ESpeechTone::Polite,   TEXT("could use a hand if you're free"));
	Add(ESpeechContext::AskForHelp, ESpeechTone::Nervous,  TEXT("please help"));
	Add(ESpeechContext::AskForHelp, ESpeechTone::Any,      TEXT("need some help over here"));

	Add(ESpeechContext::OfferHelp, ESpeechTone::Friendly, TEXT("need a hand?"));
	Add(ESpeechContext::OfferHelp, ESpeechTone::Friendly, TEXT("want some help with that?"));
	Add(ESpeechContext::OfferHelp, ESpeechTone::Polite,   TEXT("i can help if you need it"));

	Add(ESpeechContext::ThankYou, ESpeechTone::Any,      TEXT("thanks"));
	Add(ESpeechContext::ThankYou, ESpeechTone::Friendly, TEXT("thanks dude, appreciate it"));
	Add(ESpeechContext::ThankYou, ESpeechTone::Polite,   TEXT("thank you so much"));
	Add(ESpeechContext::ThankYou, ESpeechTone::Any,      TEXT("ty!"));

	Add(ESpeechContext::Apologize, ESpeechTone::Polite,   TEXT("sorry about that"));
	Add(ESpeechContext::Apologize, ESpeechTone::Polite,   TEXT("my bad"));
	Add(ESpeechContext::Apologize, ESpeechTone::Any,      TEXT("oops, sorry"));

	Add(ESpeechContext::Joke, ESpeechTone::Friendly,  TEXT("lol"));
	Add(ESpeechContext::Joke, ESpeechTone::Friendly,  TEXT("haha"));
	Add(ESpeechContext::Joke, ESpeechTone::Sarcastic, TEXT("yeah sure lol"));

	// ================================================================
	// EMOTIONAL / NEEDS (30 lines)
	// ================================================================
	Add(ESpeechContext::Hungry, ESpeechTone::Any,    TEXT("i need food"));
	Add(ESpeechContext::Hungry, ESpeechTone::Any,    TEXT("starving, gonna go find something to eat"));
	Add(ESpeechContext::Hungry, ESpeechTone::Grumpy, TEXT("so hungry i can't focus"));
	Add(ESpeechContext::Hungry, ESpeechTone::Any,    TEXT("anyone got food?"));
	Add(ESpeechContext::Hungry, ESpeechTone::Any,    TEXT("need to eat"));

	Add(ESpeechContext::Tired, ESpeechTone::Any,    TEXT("i need to rest"));
	Add(ESpeechContext::Tired, ESpeechTone::Any,    TEXT("so tired"));
	Add(ESpeechContext::Tired, ESpeechTone::Grumpy, TEXT("can't keep going like this"));
	Add(ESpeechContext::Tired, ESpeechTone::Any,    TEXT("gonna take a break"));

	Add(ESpeechContext::Happy, ESpeechTone::Friendly, TEXT("today's a good day"));
	Add(ESpeechContext::Happy, ESpeechTone::Friendly, TEXT("feeling good!"));
	Add(ESpeechContext::Happy, ESpeechTone::Any,      TEXT("this is great"));

	Add(ESpeechContext::Angry, ESpeechTone::Aggressive, TEXT("im so tilted rn"));
	Add(ESpeechContext::Angry, ESpeechTone::Aggressive, TEXT("this is ridiculous"));
	Add(ESpeechContext::Angry, ESpeechTone::Grumpy,     TEXT("whatever"));

	Add(ESpeechContext::Scared, ESpeechTone::Nervous, TEXT("i don't like this place"));
	Add(ESpeechContext::Scared, ESpeechTone::Nervous, TEXT("something doesn't feel right"));
	Add(ESpeechContext::Scared, ESpeechTone::Any,     TEXT("creepy..."));

	Add(ESpeechContext::Bored, ESpeechTone::Any,    TEXT("nothing to do around here"));
	Add(ESpeechContext::Bored, ESpeechTone::Any,    TEXT("this is boring, gonna go explore"));
	Add(ESpeechContext::Bored, ESpeechTone::Grumpy, TEXT("so bored"));
	Add(ESpeechContext::Bored, ESpeechTone::Any,    TEXT("what is there to do here"));

	Add(ESpeechContext::Excited, ESpeechTone::Friendly, TEXT("lets goooo"));
	Add(ESpeechContext::Excited, ESpeechTone::Friendly, TEXT("this is gonna be awesome"));
	Add(ESpeechContext::Excited, ESpeechTone::Any,      TEXT("oh hell yes"));
	Add(ESpeechContext::Excited, ESpeechTone::Any,      TEXT("hype"));

	Add(ESpeechContext::Frustrated, ESpeechTone::Any,        TEXT("ugh"));
	Add(ESpeechContext::Frustrated, ESpeechTone::Aggressive, TEXT("im done with this"));
	Add(ESpeechContext::Frustrated, ESpeechTone::Any,        TEXT("why does this keep happening"));
	Add(ESpeechContext::Frustrated, ESpeechTone::Grumpy,     TEXT("ffs"));

	// ================================================================
	// RELATIONSHIP (30 lines)
	// ================================================================
	Add(ESpeechContext::TrustGained, ESpeechTone::Any,      TEXT("you're alright, i like working with you"));
	Add(ESpeechContext::TrustGained, ESpeechTone::Any,      TEXT("good to have someone i can count on"));
	Add(ESpeechContext::TrustGained, ESpeechTone::Friendly, TEXT("you're cool, glad we teamed up"));
	Add(ESpeechContext::TrustGained, ESpeechTone::Polite,   TEXT("i appreciate what you did back there"));

	Add(ESpeechContext::TrustLost, ESpeechTone::Any,        TEXT("i don't trust you anymore"));
	Add(ESpeechContext::TrustLost, ESpeechTone::Any,        TEXT("we had a deal and you broke it"));
	Add(ESpeechContext::TrustLost, ESpeechTone::Aggressive, TEXT("don't talk to me"));
	Add(ESpeechContext::TrustLost, ESpeechTone::Grumpy,     TEXT("yeah we're done"));

	Add(ESpeechContext::BetrayalAccusation, ESpeechTone::Aggressive, TEXT("you stabbed me in the back"));
	Add(ESpeechContext::BetrayalAccusation, ESpeechTone::Aggressive, TEXT("i trusted you and you betrayed me"));
	Add(ESpeechContext::BetrayalAccusation, ESpeechTone::Any,        TEXT("everyone watch out, this person is a snake"));
	Add(ESpeechContext::BetrayalAccusation, ESpeechTone::Aggressive, TEXT("you're going to regret that"));

	Add(ESpeechContext::ForgivenessOffer, ESpeechTone::Polite,   TEXT("look, maybe we can start over"));
	Add(ESpeechContext::ForgivenessOffer, ESpeechTone::Friendly, TEXT("water under the bridge, lets move on"));
	Add(ESpeechContext::ForgivenessOffer, ESpeechTone::Any,      TEXT("i'll let it go this time"));

	Add(ESpeechContext::RevengeWarning, ESpeechTone::Aggressive, TEXT("i remember what you did. watch your back."));
	Add(ESpeechContext::RevengeWarning, ESpeechTone::Aggressive, TEXT("next time we meet, it won't go so well for you"));
	Add(ESpeechContext::RevengeWarning, ESpeechTone::Aggressive, TEXT("this isn't over"));
	Add(ESpeechContext::RevengeWarning, ESpeechTone::Any,        TEXT("you'll get what's coming to you"));

	Add(ESpeechContext::GrudgeReminder, ESpeechTone::Any,        TEXT("you killed me before. i haven't forgotten."));
	Add(ESpeechContext::GrudgeReminder, ESpeechTone::Aggressive, TEXT("remember me? because i remember you"));
	Add(ESpeechContext::GrudgeReminder, ESpeechTone::Grumpy,     TEXT("i still owe you for last time"));
	Add(ESpeechContext::GrudgeReminder, ESpeechTone::Any,        TEXT("we have unfinished business"));

	Add(ESpeechContext::AllianceProposal, ESpeechTone::Friendly, TEXT("hey we should work together more often"));
	Add(ESpeechContext::AllianceProposal, ESpeechTone::Polite,   TEXT("i think we make a good team"));
	Add(ESpeechContext::AllianceProposal, ESpeechTone::Any,      TEXT("wanna be allies?"));
	Add(ESpeechContext::AllianceProposal, ESpeechTone::Any,      TEXT("you watch my back, i watch yours?"));

	// ================================================================
	// ENVIRONMENTAL (35 lines)
	// ================================================================
	Add(ESpeechContext::WeatherComment, ESpeechTone::Any,      TEXT("great weather for mining"));
	Add(ESpeechContext::WeatherComment, ESpeechTone::Grumpy,   TEXT("ugh its raining again"));
	Add(ESpeechContext::WeatherComment, ESpeechTone::Any,      TEXT("love this weather"));
	Add(ESpeechContext::WeatherComment, ESpeechTone::Any,      TEXT("it's getting cold"));
	Add(ESpeechContext::WeatherComment, ESpeechTone::Friendly, TEXT("nice day out"));

	Add(ESpeechContext::LocationComment, ESpeechTone::Any,      TEXT("cool spot"));
	Add(ESpeechContext::LocationComment, ESpeechTone::Any,      TEXT("never been here before"));
	Add(ESpeechContext::LocationComment, ESpeechTone::Nervous,  TEXT("this place gives me the creeps"));
	Add(ESpeechContext::LocationComment, ESpeechTone::Friendly, TEXT("i like this area"));
	Add(ESpeechContext::LocationComment, ESpeechTone::Any,      TEXT("wonder what's over there"));

	Add(ESpeechContext::TimeOfDayComment, ESpeechTone::Any,    TEXT("getting late"));
	Add(ESpeechContext::TimeOfDayComment, ESpeechTone::Any,    TEXT("should probably head back before dark"));
	Add(ESpeechContext::TimeOfDayComment, ESpeechTone::Grumpy, TEXT("ugh its early"));
	Add(ESpeechContext::TimeOfDayComment, ESpeechTone::Any,    TEXT("still a few hours of daylight left"));

	Add(ESpeechContext::SpottedSomething, ESpeechTone::Any,      TEXT("what's that over there"));
	Add(ESpeechContext::SpottedSomething, ESpeechTone::Any,      TEXT("anyone see that?"));
	Add(ESpeechContext::SpottedSomething, ESpeechTone::Nervous,  TEXT("did you see that? something moved"));
	Add(ESpeechContext::SpottedSomething, ESpeechTone::Friendly, TEXT("hey check this out"));

	Add(ESpeechContext::SpottedDanger, ESpeechTone::Any,        TEXT("enemies ahead"));
	Add(ESpeechContext::SpottedDanger, ESpeechTone::Nervous,    TEXT("oh no theres something big over there"));
	Add(ESpeechContext::SpottedDanger, ESpeechTone::Any,        TEXT("heads up, hostiles nearby"));
	Add(ESpeechContext::SpottedDanger, ESpeechTone::Aggressive, TEXT("got company"));

	Add(ESpeechContext::SpottedResource, ESpeechTone::Any,      TEXT("found a resource node"));
	Add(ESpeechContext::SpottedResource, ESpeechTone::Friendly, TEXT("ooh look, ore deposit"));
	Add(ESpeechContext::SpottedResource, ESpeechTone::Any,      TEXT("nice, good spot for gathering"));
	Add(ESpeechContext::SpottedResource, ESpeechTone::Any,      TEXT("there's a vein over here if anyone needs ore"));

	Add(ESpeechContext::NightfallComment, ESpeechTone::Any,     TEXT("getting dark, time to head back"));
	Add(ESpeechContext::NightfallComment, ESpeechTone::Nervous, TEXT("i hate being out here at night"));
	Add(ESpeechContext::NightfallComment, ESpeechTone::Any,     TEXT("should find shelter"));
	Add(ESpeechContext::NightfallComment, ESpeechTone::Grumpy,  TEXT("great, its dark already"));
	Add(ESpeechContext::NightfallComment, ESpeechTone::Any,     TEXT("the stars are out"));

	Add(ESpeechContext::DawnComment, ESpeechTone::Any,      TEXT("new day, let's get to work"));
	Add(ESpeechContext::DawnComment, ESpeechTone::Friendly, TEXT("morning!"));
	Add(ESpeechContext::DawnComment, ESpeechTone::Grumpy,   TEXT("ugh, morning already?"));
	Add(ESpeechContext::DawnComment, ESpeechTone::Any,      TEXT("rise and grind"));
	Add(ESpeechContext::DawnComment, ESpeechTone::Any,      TEXT("another day another gold coin"));

	UE_LOG(LogNPCSpeech, Log, TEXT("Speech database initialized with %d total lines"), SpeechDatabase.Num());
}

// ============================================================================
// SPEECH TRIGGERING
// ============================================================================

bool UAoCNPCSpeech::TriggerSpeech(ESpeechContext Context)
{
	// Stealth suppresses ALL speech
	if (bInStealth) return false;

	// Global cooldown check — compute effective cooldown from talkative level
	float EffectiveCooldown = FMath::Lerp(MaxGlobalCooldown, GlobalSpeechCooldown, TalkativeLevel);
	float TimeSinceLastSpeech = GetWorld()->GetTimeSeconds() - LastGlobalSpeechTime;
	if (TimeSinceLastSpeech < EffectiveCooldown)
	{
		return false;
	}

	// Per-context cooldown
	if (IsContextOnCooldown(Context))
	{
		return false;
	}

	// Talkative trait check — probability of actually speaking
	float SpeechChance = TalkativeLevel;

	// Party boost — speak more in groups (humans chat more in groups)
	if (bIsInParty)
	{
		SpeechChance *= 1.5f;
	}

	// Alone reduction — who talks to themselves? (mostly)
	if (!bIsInParty)
	{
		SpeechChance *= 0.4f;
	}

	// Combat/urgent contexts are more likely to trigger speech
	switch (Context)
	{
	case ESpeechContext::CombatCallForHelp:
	case ESpeechContext::CombatLowHP:
	case ESpeechContext::CombatDying:
	case ESpeechContext::SpottedDanger:
	case ESpeechContext::PartyBetray:
		SpeechChance = FMath::Max(SpeechChance, 0.8f); // Urgent contexts almost always speak
		break;
	case ESpeechContext::CombatEngage:
	case ESpeechContext::CombatKilledEnemy:
	case ESpeechContext::CombatVictory:
		SpeechChance = FMath::Max(SpeechChance, 0.5f);
		break;
	default:
		break;
	}

	SpeechChance = FMath::Clamp(SpeechChance, 0.0f, 1.0f);

	if (FMath::FRand() > SpeechChance)
	{
		return false; // Didn't pass the talkative check
	}

	// Select a line
	FString Line = SelectLine(Context);
	if (Line.IsEmpty()) return false;

	// Apply typo chance for talkative NPCs
	if (TalkativeLevel > 0.7f)
	{
		Line = ApplyTypos(Line);
	}

	// Apply personality text filter
	Line = ApplyPersonalityFilter(Line);

	// Queue with a random human-like delay (0.5 - 3 seconds)
	FPendingSpeech Pending;
	Pending.Line = Line;
	Pending.DeliverAtTime = GetWorld()->GetTimeSeconds() + FMath::RandRange(MinSpeechDelay, MaxSpeechDelay);
	Pending.Context = Context;
	PendingSpeechQueue.Add(Pending);

	// Set cooldown
	SetContextCooldown(Context, GetCooldownForContext(Context));

	return true;
}

bool UAoCNPCSpeech::TriggerSpeechWithTarget(ESpeechContext Context, const FString& TargetEntityId)
{
	// For now, use the same path as TriggerSpeech.
	// In a more advanced system, we could personalize the line based on the target.
	return TriggerSpeech(Context);
}

void UAoCNPCSpeech::ForceSpeech(ESpeechContext Context)
{
	FString Line = SelectLine(Context);
	if (Line.IsEmpty()) return;

	if (TalkativeLevel > 0.7f)
	{
		Line = ApplyTypos(Line);
	}
	Line = ApplyPersonalityFilter(Line);

	// Deliver immediately with minimal delay
	FPendingSpeech Pending;
	Pending.Line = Line;
	Pending.DeliverAtTime = GetWorld()->GetTimeSeconds() + FMath::RandRange(0.2f, 0.8f);
	Pending.Context = Context;
	PendingSpeechQueue.Add(Pending);
}

void UAoCNPCSpeech::SayExactLine(const FString& Line)
{
	FPendingSpeech Pending;
	Pending.Line = Line;
	Pending.DeliverAtTime = GetWorld()->GetTimeSeconds() + FMath::RandRange(MinSpeechDelay, MaxSpeechDelay);
	Pending.Context = ESpeechContext::SmallTalk;
	PendingSpeechQueue.Add(Pending);
}

// ============================================================================
// LINE SELECTION
// ============================================================================

FString UAoCNPCSpeech::SelectLine(ESpeechContext Context) const
{
	// Build a list of candidate lines matching this context
	TArray<int32> Candidates;

	for (int32 i = 0; i < SpeechDatabase.Num(); i++)
	{
		const FSpeechLine& Line = SpeechDatabase[i];

		if (Line.Context != Context) continue;

		// Tone matching: select lines that match our personality OR are "Any"
		if (Line.Tone != ESpeechTone::Any && Line.Tone != DominantTone)
		{
			// 20% chance to still use a mismatched tone (people aren't 100% consistent)
			if (FMath::FRand() > 0.2f) continue;
		}

		// Skip recently used lines
		bool bRecentlyUsed = false;
		for (int32 RecentIdx : RecentlyUsedLines)
		{
			if (RecentIdx == i)
			{
				bRecentlyUsed = true;
				break;
			}
		}
		if (bRecentlyUsed) continue;

		Candidates.Add(i);
	}

	if (Candidates.Num() == 0)
	{
		// Fallback: allow recently used lines if we have no other options
		for (int32 i = 0; i < SpeechDatabase.Num(); i++)
		{
			if (SpeechDatabase[i].Context == Context)
			{
				if (SpeechDatabase[i].Tone == ESpeechTone::Any || SpeechDatabase[i].Tone == DominantTone)
				{
					Candidates.Add(i);
				}
			}
		}
	}

	if (Candidates.Num() == 0) return FString();

	// Pick a random candidate
	int32 SelectedIdx = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	// Record as recently used (mutable operation via const_cast — acceptable for selection tracking)
	const_cast<UAoCNPCSpeech*>(this)->RecentlyUsedLines[RecentLineIndex % MaxRecentLines] = SelectedIdx;
	const_cast<UAoCNPCSpeech*>(this)->RecentLineIndex++;

	return SpeechDatabase[SelectedIdx].Line;
}

FString UAoCNPCSpeech::ApplyTypos(const FString& Line) const
{
	// Only apply typos occasionally — ~1.2% chance per character
	FString Result = Line;

	// Common typo patterns
	struct FTypoRule
	{
		const TCHAR* From;
		const TCHAR* To;
	};

	static const TArray<FTypoRule> TypoRules = {
		{TEXT("the"), TEXT("teh")},
		{TEXT("nice"), TEXT("ncie")},
		{TEXT("that"), TEXT("taht")},
		{TEXT("with"), TEXT("wiht")},
		{TEXT("this"), TEXT("htis")},
		{TEXT("have"), TEXT("ahve")},
		{TEXT("some"), TEXT("soem")},
		{TEXT("here"), TEXT("heer")},
		{TEXT("from"), TEXT("form")},
		{TEXT("just"), TEXT("jsut")},
		{TEXT("good"), TEXT("godo")},
		{TEXT("back"), TEXT("bakc")},
	};

	// 1.2% chance per word to have a typo
	for (const FTypoRule& Rule : TypoRules)
	{
		if (Result.Contains(Rule.From) && FMath::FRand() < TypoChance * 100.0f)
		{
			Result = Result.Replace(Rule.From, Rule.To, ESearchCase::CaseSensitive);
			break; // Only one typo per message
		}
	}

	return Result;
}

FString UAoCNPCSpeech::ApplyPersonalityFilter(const FString& Line) const
{
	FString Result = Line;

	switch (DominantTone)
	{
	case ESpeechTone::Aggressive:
		// Aggressive NPCs type in lowercase, sometimes add "lol" at the end
		Result = Result.ToLower();
		break;

	case ESpeechTone::Polite:
		// Polite NPCs capitalize the first letter
		if (Result.Len() > 0)
		{
			Result[0] = FChar::ToUpper(Result[0]);
		}
		break;

	case ESpeechTone::Grumpy:
		// Grumpy NPCs use lowercase and shorter sentences
		Result = Result.ToLower();
		break;

	default:
		// Random capitalization style
		if (FMath::FRand() < 0.3f) // 30% chance to be lowercase (common in games)
		{
			Result = Result.ToLower();
		}
		break;
	}

	return Result;
}

// ============================================================================
// DELIVERY
// ============================================================================

void UAoCNPCSpeech::DeliverLine(const FString& Line, ESpeechContext Context)
{
	if (!OwnerNPC || Line.IsEmpty()) return;

	FString SpeakerName = OwnerNPC->GetName();

	// TODO: Output to Local Chat channel via the game's chat system
	// In the real implementation, this calls the chat manager:
	// UChatManager::Get()->SendLocalMessage(SpeakerName, Line, OwnerNPC->GetActorLocation(), LocalChatRadius);

	UE_LOG(LogNPCSpeech, Log, TEXT("[LOCAL] %s: %s"), *SpeakerName, *Line);

	LastGlobalSpeechTime = GetWorld()->GetTimeSeconds();
	TotalLinesSpokeCount++;

	OnNPCSpeech.Broadcast(SpeakerName, Line, Context);
}

void UAoCNPCSpeech::TickPendingSpeech(float DeltaTime)
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	for (int32 i = PendingSpeechQueue.Num() - 1; i >= 0; i--)
	{
		if (CurrentTime >= PendingSpeechQueue[i].DeliverAtTime)
		{
			DeliverLine(PendingSpeechQueue[i].Line, PendingSpeechQueue[i].Context);
			PendingSpeechQueue.RemoveAt(i);
		}
	}
}

// ============================================================================
// COOLDOWNS
// ============================================================================

bool UAoCNPCSpeech::IsContextOnCooldown(ESpeechContext Context) const
{
	const FSpeechCooldown* CD = ContextCooldowns.Find(Context);
	if (!CD) return false;

	float TimeSince = GetWorld()->GetTimeSeconds() - CD->LastSpokeTime;
	return TimeSince < CD->CooldownDuration;
}

void UAoCNPCSpeech::SetContextCooldown(ESpeechContext Context, float Duration)
{
	FSpeechCooldown& CD = ContextCooldowns.FindOrAdd(Context);
	CD.LastSpokeTime = GetWorld()->GetTimeSeconds();
	CD.CooldownDuration = Duration;
}

float UAoCNPCSpeech::GetCooldownForContext(ESpeechContext Context) const
{
	// Different contexts have different cooldowns to prevent spam
	switch (Context)
	{
	// Combat — moderate cooldown (don't spam during fights)
	case ESpeechContext::CombatEngage:
	case ESpeechContext::CombatTaunt:
	case ESpeechContext::CombatKilledEnemy:
		return 20.0f;

	// Urgent combat — shorter cooldown (these are important)
	case ESpeechContext::CombatLowHP:
	case ESpeechContext::CombatCallForHelp:
		return 10.0f;

	// One-time combat events
	case ESpeechContext::CombatDying:
	case ESpeechContext::CombatVictory:
	case ESpeechContext::PartyBetray:
		return 60.0f;

	// Gathering — long cooldown (you don't comment on every swing)
	case ESpeechContext::MiningFound:
	case ESpeechContext::MiningTired:
	case ESpeechContext::WoodcuttingChop:
	case ESpeechContext::GatheringHerb:
		return 120.0f;

	// Crafting events — moderate (one comment per craft)
	case ESpeechContext::CraftingSuccess:
	case ESpeechContext::CraftingFailed:
	case ESpeechContext::CraftingStarting:
	case ESpeechContext::CookingDone:
	case ESpeechContext::SmeltingDone:
		return 60.0f;

	// Fishing — moderate
	case ESpeechContext::FishingBite:
	case ESpeechContext::FishingNothing:
	case ESpeechContext::FishingCaught:
		return 45.0f;

	// Social — long cooldown
	case ESpeechContext::SmallTalk:
	case ESpeechContext::Gossip:
		return 180.0f;

	// Needs — moderate (don't constantly whine)
	case ESpeechContext::Hungry:
	case ESpeechContext::Tired:
	case ESpeechContext::Bored:
		return 300.0f;

	// Environmental — long cooldown
	case ESpeechContext::WeatherComment:
	case ESpeechContext::LocationComment:
	case ESpeechContext::TimeOfDayComment:
	case ESpeechContext::NightfallComment:
	case ESpeechContext::DawnComment:
		return 600.0f;

	// Relationship — long cooldown
	case ESpeechContext::TrustGained:
	case ESpeechContext::TrustLost:
	case ESpeechContext::GrudgeReminder:
	case ESpeechContext::RevengeWarning:
		return 300.0f;

	// Warning/spotted — moderate
	case ESpeechContext::Warning:
	case ESpeechContext::SpottedDanger:
	case ESpeechContext::SpottedResource:
	case ESpeechContext::SpottedSomething:
		return 30.0f;

	// Party loot — moderate
	case ESpeechContext::PartyLootFair:
	case ESpeechContext::PartyLootUnfair:
	case ESpeechContext::PartyLootAngry:
		return 60.0f;

	// Trading
	case ESpeechContext::TradeOffer:
	case ESpeechContext::TradeAccept:
	case ESpeechContext::TradeDecline:
	case ESpeechContext::TradeHaggle:
	case ESpeechContext::TradeThanks:
		return 30.0f;

	default:
		return 30.0f;
	}
}

bool UAoCNPCSpeech::IsOnGlobalCooldown() const
{
	float EffectiveCooldown = FMath::Lerp(MaxGlobalCooldown, GlobalSpeechCooldown, TalkativeLevel);
	return (GetWorld()->GetTimeSeconds() - LastGlobalSpeechTime) < EffectiveCooldown;
}

// ============================================================================
// QUERIES
// ============================================================================

int32 UAoCNPCSpeech::GetLineCountForContext(ESpeechContext Context) const
{
	int32 Count = 0;
	for (const FSpeechLine& Line : SpeechDatabase)
	{
		if (Line.Context == Context) Count++;
	}
	return Count;
}

FString UAoCNPCSpeech::GetDebugString() const
{
	FString Result;
	Result += TEXT("=== Speech System ===\n");
	Result += FString::Printf(TEXT("Total Lines: %d | Spoken: %d | Talkative: %.2f | Tone: %d\n"),
		SpeechDatabase.Num(), TotalLinesSpokeCount, TalkativeLevel, (int32)DominantTone);
	Result += FString::Printf(TEXT("Stealth: %s | InParty: %s | GlobalCD: %s\n"),
		bInStealth ? TEXT("YES") : TEXT("NO"),
		bIsInParty ? TEXT("YES") : TEXT("NO"),
		IsOnGlobalCooldown() ? TEXT("ACTIVE") : TEXT("READY"));
	Result += FString::Printf(TEXT("Pending queue: %d\n"), PendingSpeechQueue.Num());
	return Result;
}
