// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GamebeastTypes.h"
#include "Internal/GamebeastEnvironment.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastMarkerQueue.h"
#include "Internal/GamebeastPlatformFacts.h"

// EAutomationTestFlags is a namespace before UE 5.5 and an enum class after; a macro fits both.
#define GAMEBEAST_TEST_FLAGS (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

namespace
{
	/** Records batches and lets the test answer each one. */
	struct FFakeTransport
	{
		struct FCall
		{
			TArray<TSharedPtr<FJsonValue>> Markers;
			TFunction<void(const FGamebeastHttpResponse&)> Complete;
		};
		TArray<FCall> Calls;

		FGamebeastMarkerQueue::FSendFn AsSendFn()
		{
			return [this](const TArray<TSharedPtr<FJsonValue>>& Markers, TFunction<void(const FGamebeastHttpResponse&)> Complete)
			{
				Calls.Add({ Markers, MoveTemp(Complete) });
			};
		}

		void Respond(int32 Index, int32 Status)
		{
			FGamebeastHttpResponse Response;
			Response.Status = Status;
			Calls[Index].Complete(Response);
		}

		void RespondTransportError(int32 Index)
		{
			FGamebeastHttpResponse Response;
			Response.bTransportError = true;
			Calls[Index].Complete(Response);
		}
	};

	TSharedPtr<FJsonValue> Marker(int32 Index)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("index"), Index);
		return MakeShared<FJsonValueObject>(Object);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamebeastEnvironmentTest, "Gamebeast.Environment", GAMEBEAST_TEST_FLAGS)

bool FGamebeastEnvironmentTest::RunTest(const FString& Parameters)
{
	using namespace Gamebeast::Environment;

	TestEqual(TEXT("slug: spaces"), ToAliasSlug(TEXT("My Config")), FString(TEXT("my-config")));
	TestEqual(TEXT("slug: punctuation"), ToAliasSlug(TEXT("Combat (PvP)!")), FString(TEXT("combat-pvp")));
	TestEqual(TEXT("slug: trims"), ToAliasSlug(TEXT("  spaced  ")), FString(TEXT("spaced")));
	TestEqual(TEXT("slug: collapses dashes"), ToAliasSlug(TEXT("a -- b")), FString(TEXT("a-b")));
	TestEqual(TEXT("slug: nothing left"), ToAliasSlug(TEXT("!!!")), FString());

	TestEqual(TEXT("Auto in editor"), Resolve(EGamebeastEnvironment::Auto, FString(), true), Development);
	TestEqual(TEXT("Auto in a build"), Resolve(EGamebeastEnvironment::Auto, FString(), false), Production);
	TestEqual(TEXT("explicit production"), Resolve(EGamebeastEnvironment::Production, FString(), true), Production);
	TestEqual(TEXT("custom overrides"), Resolve(EGamebeastEnvironment::Production, TEXT("Staging"), false), FString(TEXT("staging")));
	TestEqual(TEXT("studio is development"), Resolve(EGamebeastEnvironment::Production, TEXT("studio"), false), Development);

	// The Unity SDK sent isstudio=false for development, so its development markers landed in
	// production. Development must be true.
	bool bSupported = false;
	TestFalse(TEXT("production is not studio"), ToLegacyIsStudio(Production, bSupported));
	TestTrue(TEXT("production supported"), bSupported);
	TestTrue(TEXT("development is studio"), ToLegacyIsStudio(Development, bSupported));
	TestTrue(TEXT("development supported"), bSupported);
	TestTrue(TEXT("custom falls back to studio"), ToLegacyIsStudio(TEXT("staging"), bSupported));
	TestFalse(TEXT("custom unsupported on v1"), bSupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamebeastJsonTest, "Gamebeast.Json", GAMEBEAST_TEST_FLAGS)

bool FGamebeastJsonTest::RunTest(const FString& Parameters)
{
	using namespace Gamebeast::Json;

	const TSharedPtr<FJsonValue> Root = ParseValue(TEXT(R"({"Shop":{"Prices":[10,20]},"Speed":5.5,"Name":"x"})"));
	TestTrue(TEXT("parsed"), Root.IsValid());

	FString Alias;
	TArray<FString> Segments;
	TestTrue(TEXT("split"), SplitConfigPath(TEXT("MyConfig.Shop.Prices.1"), Alias, Segments));
	TestEqual(TEXT("alias"), Alias, FString(TEXT("MyConfig")));
	TestEqual(TEXT("segments"), Segments.Num(), 3);
	TestFalse(TEXT("empty alias"), SplitConfigPath(TEXT(".Speed"), Alias, Segments));

	const TSharedPtr<FJsonValue> Price = Resolve(Root, { TEXT("Shop"), TEXT("Prices"), TEXT("1") });
	TestTrue(TEXT("array index"), Price.IsValid() && Price->AsNumber() == 20.0);
	TestFalse(TEXT("out of range"), Resolve(Root, { TEXT("Shop"), TEXT("Prices"), TEXT("2") }).IsValid());
	TestFalse(TEXT("negative is a key"), Resolve(Root, { TEXT("Shop"), TEXT("Prices"), TEXT("-1") }).IsValid());
	TestFalse(TEXT("missing key"), Resolve(Root, { TEXT("Nope") }).IsValid());
	TestTrue(TEXT("bare alias is the document"), Resolve(Root, {}) == Root);

	TestTrue(TEXT("deep equal"), DeepEquals(ParseValue(TEXT(R"({"a":[1,{"b":true}]})")), ParseValue(TEXT(R"({"a":[1,{"b":true}]})"))));
	TestFalse(TEXT("deep differ"), DeepEquals(ParseValue(TEXT(R"({"a":[1]})")), ParseValue(TEXT(R"({"a":[2]})"))));
	TestTrue(TEXT("null equals missing"), DeepEquals(MakeShared<FJsonValueNull>(), nullptr));

	// The Unity SDK sent dashless GUIDs, which the backend's z.uuid() rejects and replaces.
	const FString Uuid = NewUuid();
	TestEqual(TEXT("uuid length"), Uuid.Len(), 36);
	TestTrue(TEXT("uuid dashes"), Uuid[8] == TEXT('-') && Uuid[13] == TEXT('-'));
	TestEqual(TEXT("uuid lowercase"), Uuid, Uuid.ToLower());

	TestTrue(TEXT("milliseconds, not seconds"), NowUnixMilliseconds() > 1000000000000LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamebeastPropertiesTest, "Gamebeast.Properties", GAMEBEAST_TEST_FLAGS)

bool FGamebeastPropertiesTest::RunTest(const FString& Parameters)
{
	FGamebeastProperties Original = FGamebeastProperties()
		.Set(TEXT("Score"), 600)
		.Set(TEXT("Map"), TEXT("Office"))
		.Set(TEXT("Won"), true)
		.Set(TEXT("Position"), FVector(1.0, 2.0, 3.0));

	TestEqual(TEXT("four fields"), Original.Num(), 4);
	TestEqual(TEXT("vector as array"),
		Gamebeast::Json::SerializeValue(Original.ToJson()->TryGetField(TEXT("Position"))), FString(TEXT("[1,2,3]")));

	FGamebeastProperties Copy = Original;
	Copy.Set(TEXT("Extra"), 1);
	TestEqual(TEXT("copy changed"), Copy.Num(), 5);
	TestEqual(TEXT("original untouched"), Original.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamebeastMarkerQueueTest, "Gamebeast.MarkerQueue", GAMEBEAST_TEST_FLAGS)

bool FGamebeastMarkerQueueTest::RunTest(const FString& Parameters)
{
	// Dropped batches are logged as errors on purpose.
	AddExpectedError(TEXT("Dropped"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Marker buffer full"), EAutomationExpectedErrorFlags::Contains, 0);

	FGamebeastMarkerQueue::FOptions Options;
	Options.MaxBatchSize = 3;
	Options.FlushIntervalSeconds = 10.f;
	Options.MaxBufferedMarkers = 5;
	Options.MaxSendAttempts = 3;
	Options.RetryBaseDelaySeconds = 2.f;

	// Batches by size and by time.
	{
		FFakeTransport Transport;
		FGamebeastMarkerQueue Queue(Transport.AsSendFn(), Options);
		Queue.Enqueue(Marker(1));
		Queue.Enqueue(Marker(2));
		TestEqual(TEXT("waits for a full batch"), Transport.Calls.Num(), 0);
		Queue.Enqueue(Marker(3));
		TestEqual(TEXT("full batch sends"), Transport.Calls.Num(), 1);
		TestEqual(TEXT("batch size"), Transport.Calls[0].Markers.Num(), 3);

		Queue.Enqueue(Marker(4));
		Queue.Tick(9.f);
		TestEqual(TEXT("not yet due"), Transport.Calls.Num(), 1);
		Queue.Tick(1.f);
		TestEqual(TEXT("interval sends"), Transport.Calls.Num(), 2);
	}

	// Retries transient failures with backoff, then gives up.
	{
		FFakeTransport Transport;
		FGamebeastMarkerQueue Queue(Transport.AsSendFn(), Options);
		Queue.Enqueue(Marker(1));
		Queue.Flush();
		Transport.Respond(0, 503);
		TestEqual(TEXT("queued for retry"), Queue.BufferedCount(), 1);
		Queue.Tick(1.9f);
		TestEqual(TEXT("backoff 2s"), Transport.Calls.Num(), 1);
		Queue.Tick(0.2f);
		TestEqual(TEXT("retried"), Transport.Calls.Num(), 2);
		Transport.RespondTransportError(1);
		Queue.Tick(4.1f);
		TestEqual(TEXT("backoff 4s"), Transport.Calls.Num(), 3);
		Transport.Respond(2, 500);
		TestEqual(TEXT("dropped after max attempts"), Queue.BufferedCount(), 0);
	}

	// Permanent rejections are dropped, not retried.
	{
		FFakeTransport Transport;
		FGamebeastMarkerQueue Queue(Transport.AsSendFn(), Options);
		Queue.Enqueue(Marker(1));
		Queue.Flush();
		Transport.Respond(0, 400);
		TestEqual(TEXT("400 dropped"), Queue.BufferedCount(), 0);
		Queue.Tick(60.f);
		TestEqual(TEXT("never resent"), Transport.Calls.Num(), 1);
	}

	// TakeUnsent hands everything not in flight to the caller, exactly once.
	{
		FFakeTransport Transport;
		FGamebeastMarkerQueue Queue(Transport.AsSendFn(), Options);
		Queue.Enqueue(Marker(1));
		Queue.Flush();
		Transport.Respond(0, 429);
		Queue.Enqueue(Marker(2));
		const TArray<TSharedPtr<FJsonValue>> Unsent = Queue.TakeUnsent();
		TestEqual(TEXT("retry + pending"), Unsent.Num(), 2);
		TestEqual(TEXT("queue emptied"), Queue.BufferedCount(), 0);
		Queue.Tick(60.f);
		TestEqual(TEXT("nothing sent twice"), Transport.Calls.Num(), 1);
	}

	// The buffer cap drops the oldest markers.
	{
		FFakeTransport Transport;
		FGamebeastMarkerQueue::FOptions Capped = Options;
		Capped.MaxBatchSize = 100;
		Capped.MaxBufferedMarkers = 3;
		FGamebeastMarkerQueue Queue(Transport.AsSendFn(), Capped);
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			Queue.Enqueue(Marker(Index));
		}
		const TArray<TSharedPtr<FJsonValue>> Unsent = Queue.TakeUnsent();
		TestEqual(TEXT("capped"), Unsent.Num(), 3);
		TestEqual(TEXT("oldest dropped"), static_cast<int32>(Unsent[0]->AsObject()->GetNumberField(TEXT("index"))), 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamebeastPlatformFactsTest, "Gamebeast.PlatformFacts", GAMEBEAST_TEST_FLAGS)

bool FGamebeastPlatformFactsTest::RunTest(const FString& Parameters)
{
	using namespace Gamebeast::PlatformFacts;

	// The vocabulary registered for `unreal.platform` / `unreal.device` in @gamebeast/experiments.
	TestEqual(TEXT("Windows"), NormalizePlatform(TEXT("Windows")), TEXT("windows"));
	TestEqual(TEXT("WinGDK"), NormalizePlatform(TEXT("WinGDK")), TEXT("windows"));
	TestEqual(TEXT("Mac"), NormalizePlatform(TEXT("Mac")), TEXT("mac"));
	TestEqual(TEXT("LinuxArm64"), NormalizePlatform(TEXT("LinuxArm64")), TEXT("linux"));
	TestEqual(TEXT("IOS"), NormalizePlatform(TEXT("IOS")), TEXT("ios"));
	TestEqual(TEXT("TVOS"), NormalizePlatform(TEXT("TVOS")), TEXT("ios"));
	TestEqual(TEXT("Android"), NormalizePlatform(TEXT("Android")), TEXT("android"));
	TestEqual(TEXT("PS5"), NormalizePlatform(TEXT("PS5")), TEXT("playstation"));
	TestEqual(TEXT("XSX"), NormalizePlatform(TEXT("XSX")), TEXT("xbox"));
	TestEqual(TEXT("XboxOneGDK"), NormalizePlatform(TEXT("XboxOneGDK")), TEXT("xbox"));
	TestEqual(TEXT("Switch"), NormalizePlatform(TEXT("Switch")), TEXT("switch"));
	TestEqual(TEXT("Unknown name"), NormalizePlatform(TEXT("HoloLens")), TEXT("unknown"));
	TestEqual(TEXT("Empty name"), NormalizePlatform(TEXT("")), TEXT("unknown"));

	TestEqual(TEXT("PC device"), DeviceForPlatform(TEXT("linux")), TEXT("pc"));
	TestEqual(TEXT("Mobile device"), DeviceForPlatform(TEXT("android")), TEXT("mobile"));
	TestEqual(TEXT("Console device"), DeviceForPlatform(TEXT("xbox")), TEXT("console"));
	TestEqual(TEXT("Unknown device"), DeviceForPlatform(TEXT("unknown")), TEXT("unknown"));

	// ISO 639-1 lowercase and ISO 3166-1 alpha-2 uppercase, as the dashboard's vocabularies.
	TestEqual(TEXT("Language en-US"), LanguageFromLocale(TEXT("en-US")), TEXT("en"));
	TestEqual(TEXT("Language POSIX"), LanguageFromLocale(TEXT("pt_BR.UTF-8")), TEXT("pt"));
	TestEqual(TEXT("Language bare"), LanguageFromLocale(TEXT("FR")), TEXT("fr"));
	TestEqual(TEXT("Language three letters"), LanguageFromLocale(TEXT("fil-PH")), TEXT("unknown"));
	TestEqual(TEXT("Language empty"), LanguageFromLocale(TEXT("")), TEXT("unknown"));

	TestEqual(TEXT("Country en-US"), CountryFromLocale(TEXT("en-US")), TEXT("US"));
	TestEqual(TEXT("Country after script"), CountryFromLocale(TEXT("zh-Hans-CN")), TEXT("CN"));
	TestEqual(TEXT("Country POSIX"), CountryFromLocale(TEXT("de_de.UTF-8@euro")), TEXT("DE"));
	TestEqual(TEXT("Country missing"), CountryFromLocale(TEXT("en")), TEXT("unknown"));
	TestEqual(TEXT("Country numeric region"), CountryFromLocale(TEXT("es-419")), TEXT("unknown"));

	// Only registered keys: the backend rejects anything else in the namespace.
	const TSharedRef<FJsonObject> Facts = Collect();
	TArray<FString> Keys;
	Facts->Values.GetKeys(Keys);
	Keys.Sort();
	TestEqual(TEXT("Namespace keys"), FString::Join(Keys, TEXT(",")),
		FString(TEXT("appVersion,country,device,engineVersion,language,platform")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
