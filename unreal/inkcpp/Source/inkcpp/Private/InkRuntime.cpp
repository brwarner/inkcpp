// Fill out your copyright notice in the Description page of Project Settings.

#include "InkRuntime.h"

// Game includes
#include "inkcpp.h"
#include "InkThread.h"
#include "TagList.h"
#include "InkAsset.h"

// inkcpp includes
#include "ink/story.h"
#include "ink/globals.h"

// Sets default values
AInkRuntime::AInkRuntime() : mpRuntime(nullptr)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

AInkRuntime::~AInkRuntime()
{

}

// Called when the game starts or when spawned
void AInkRuntime::BeginPlay()
{
	Super::BeginPlay();

	// Create the CPU for the story
	if (InkAsset != nullptr)
	{
		// TODO: Better error handling? What if load fails.
		mpRuntime = ink::runtime::story::from_binary(reinterpret_cast<unsigned char*>(InkAsset->CompiledStory.GetData()), InkAsset->CompiledStory.Num(), false);
		UE_LOG(InkCpp, Display, TEXT("Loaded Ink asset"));

		// create globals
		mpGlobals = mpRuntime->new_globals();
	}
	else
	{
		UE_LOG(InkCpp, Warning, TEXT("No story asset assigned."));
	}
}

// Called every frame
void AInkRuntime::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Early out: no threads.
	if (mThreads.Num() == 0)
		return;

	// Special: If we're in exclusive mode, only execute the thread at the 
	//  top of the exclusive stack
	while (mExclusiveStack.Num() > 0)
	{
		// Make sure top thread is active
		UInkThread* top = mExclusiveStack.Top();

		// If it can't execute, we're not doing anything else
		if (!top->CanExecute())
			return;

		// Execute it
		if (top->Execute())
		{
			mExclusiveStack.Remove(top);
			mThreads.Remove(top);

			// execute next exclusive thread
			continue;
		}
		
		// Nothing more to do
		return;
	}

	// Execute other available threads
	for (auto iter = mThreads.CreateIterator(); iter; iter++)
	{
		UInkThread* pNextThread = *iter;

		// Ignore threads that aren't eligable for execution
		if (!pNextThread->CanExecute())
			continue;

		// Execute
		if (pNextThread->Execute())
		{
			// If the thread has finished, destroy it
			iter.RemoveCurrent();
			mExclusiveStack.Remove(pNextThread);
		}
	}
}

/*void AInkRuntime::ExternalFunctionRegistered(FString functionName)
{
	// Add to registered function set
	bool alreadyRegistered = false;
	mRegisteredFunctions.Add(functionName, &alreadyRegistered);

	// If we've never head of this function before, register it with the Story runtime
	if (!alreadyRegistered) 
	{
		FExternalFunctionHandler handler;
		handler.BindDynamic(this, &AInkRuntime::ExternalFunctionHandler);
		mpRuntime->RegisterExternalFunction(functionName, handler);
	}
}*/

UTagList* AInkRuntime::GetTagsAtPath(FString Path)
{
	// TODO
	// Get tags at knot
	//auto tags = mpRuntime->TagsForContentAtPath(Path);

	// Convert to a tag list
	UTagList* result = NewObject<UTagList>(this, UTagList::StaticClass());
	//result->Initialize(tags);

	return result;
}

/*FInkVar AInkRuntime::ExternalFunctionHandler(FString functionName, TArray<FInkVar> arguments)
{
	checkf(mpCurrentThread != nullptr, TEXT("Got callback for external function '%s' but no thread is currently active!"), *functionName);

	FInkVar result;
	bool handled = mpCurrentThread->HandleExternalFunction(functionName, arguments, result);
	if (!handled)
	{
		UE_LOG(InkRuntime, Warning, TEXT("Currently running thread has no implementation of external function %s"), *functionName);
		return FInkVar();
	}

	return result;
}*/

UInkThread* AInkRuntime::Start(TSubclassOf<class UInkThread> type, FString path, bool startImmediately)
{
	if (mpRuntime == nullptr || type == nullptr)
	{
		return nullptr;
	}

	// Instantiate the thread
	UInkThread* pThread = NewObject<UInkThread>(this, type);

	// Startup
	return StartExisting(pThread, path, startImmediately);
}

UInkThread* AInkRuntime::StartExisting(UInkThread* thread, FString path, bool startImmediately /*= true*/)
{
	if (mpRuntime == nullptr)
	{
		return nullptr;
	}

	// Initialize thread with new runner
	ink::runtime::runner runner = mpRuntime->new_runner(mpGlobals);
	thread->Initialize(path, this, runner);

	// If we're not starting immediately, just queue
	if (!startImmediately || 
		// Even if we want to start immediately, don't if there's an exclusive thread and it's not us
		(mExclusiveStack.Num() > 0 && mExclusiveStack.Top() != thread))
	{
		mThreads.Add(thread);
		return thread;
	}

	// Execute the newly created thread
	if (!thread->Execute())
	{
		// If it hasn't finished immediately, add it to the threads list
		mThreads.Add(thread);
	}

	return thread;
}

void AInkRuntime::RegisterGlobalTagFunction(FName FunctionName, const FGlobalTagFunctionDelegate& Function)
{
	// Register tag function
	mGlobalTagFunctions.FindOrAdd(FunctionName).Add(Function);
}

void AInkRuntime::HandleTagFunction(UInkThread* Caller, const TArray<FString>& Params)
{
	// Look for method and execute with parameters
	FGlobalTagFunctionMulticastDelegate* function = mGlobalTagFunctions.Find(FName(*Params[0]));
	if (function != nullptr)
	{
#define GET_PARAM(n) Params.Num() > n ? Params[n] : FString()
		function->Broadcast(Caller, GET_PARAM(1), GET_PARAM(2), GET_PARAM(3));
#undef GET_PARAM
	}
}

void AInkRuntime::PushExclusiveThread(UInkThread* Thread)
{
	// If we're already on the stack, ignore
	if (mExclusiveStack.Find(Thread) != INDEX_NONE)
		return;

	// Push
	mExclusiveStack.Push(Thread);
}

void AInkRuntime::PopExclusiveThread(UInkThread* Thread)
{
	// Remove from the stack
	mExclusiveStack.Remove(Thread);
}
