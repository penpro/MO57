// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Render/VoxelRenderComponentCreator.h"
#include "Render/VoxelRenderChunk.h"
#include "Render/VoxelMeshComponent.h"
#include "Render/VoxelRenderSubsystem.h"
#include "VoxelRuntime.h"
#include "VoxelMeshRenderProxy.h"
#include "Nanite/VoxelNaniteComponent.h"
#include "Collision/VoxelCollisionComponent.h"
#include "Collision/VoxelStaticMeshCollisionComponent.h"

FVoxelRenderComponentCreator::FVoxelRenderComponentCreator(
	FVoxelRuntime& Runtime,
	const FVoxelRenderSubsystem& Subsystem)
	: Runtime(Runtime)
	, Config(Subsystem.GetConfig())
	, Subsystem(Subsystem)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelRenderComponentCreator::ProcessRenderDatasToDestroy(const TVoxelChunkedArray<TSharedPtr<FVoxelRenderChunkData>>& RenderDatasToDestroy)
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<FVoxelRenderChunkData>& RenderData : RenderDatasToDestroy)
	{
		if (UVoxelMeshComponent* Component = RenderData->MeshComponent.Resolve())
		{
			MeshComponents.Add(Component);
		}

		if (UVoxelNaniteComponent* Component = RenderData->NaniteComponent.Resolve())
		{
			NaniteComponents.Add(Component);
		}

		if (UVoxelCollisionComponent* Component = RenderData->CollisionComponent.Resolve())
		{
			CollisionComponents.Add(Component);
		}

		if (UVoxelStaticMeshCollisionComponent* Component = RenderData->StaticMeshCollisionComponent.Resolve())
		{
			StaticMeshCollisionComponents.Add(Component);
		}
	}
}

void FVoxelRenderComponentCreator::ProcessRenderDatasToRender(const TVoxelChunkedArray<TSharedPtr<FVoxelRenderChunkData>>& RenderDatasToRender)
{
	VOXEL_FUNCTION_COUNTER();

	for (const TSharedPtr<FVoxelRenderChunkData>& RenderData : RenderDatasToRender)
	{
		if (RenderData->Collider)
		{
			UVoxelCollisionComponent* Component = RenderData->CollisionComponent.Resolve();
			if (!Component)
			{
				Component = CollisionComponents.Num() > 0
					? CollisionComponents.Pop()
					: Runtime.NewComponent<UVoxelCollisionComponent>();

				RenderData->CollisionComponent = Component;
			}
			if (!ensure(Component))
			{
				continue;
			}

			CollisionTasks.Add(FCollisionTask
			{
				Component,
				RenderData.Get()
			});
		}
		else
		{
			if (UVoxelCollisionComponent* Component = RenderData->CollisionComponent.Resolve())
			{
				CollisionComponents.Add(Component);
			}
		}

		if (Config.bIsEditorWorld &&
			RenderData->Collider)
		{
			UVoxelStaticMeshCollisionComponent* Component = RenderData->StaticMeshCollisionComponent.Resolve();
			if (!Component)
			{
				Component = StaticMeshCollisionComponents.Num() > 0
					? StaticMeshCollisionComponents.Pop()
					: Runtime.NewComponent<UVoxelStaticMeshCollisionComponent>();

				RenderData->StaticMeshCollisionComponent = Component;
			}
			if (!ensure(Component))
			{
				continue;
			}

			StaticMeshCollisionTasks.Add(FStaticMeshCollisionTask
			{
				Component,
				RenderData.Get()
			});
		}
		else
		{
			if (UVoxelStaticMeshCollisionComponent* Component = RenderData->StaticMeshCollisionComponent.Resolve())
			{
				StaticMeshCollisionComponents.Add(Component);
			}
		}

		if (RenderData->NaniteMesh)
		{
			UVoxelNaniteComponent* Component = RenderData->NaniteComponent.Resolve();
			if (!Component)
			{
				Component = NaniteComponents.Num() > 0
					? NaniteComponents.Pop()
					: Runtime.NewComponent<UVoxelNaniteComponent>();

				RenderData->NaniteComponent = Component;
			}
			if (!ensure(Component))
			{
				continue;
			}

			NaniteTasks.Add(FNaniteTask
			{
				Component,
				RenderData.Get()
			});
		}
		else
		{
			if (UVoxelNaniteComponent* Component = RenderData->NaniteComponent.Resolve())
			{
				NaniteComponents.Add(Component);
			}
		}

		if (RenderData->RenderProxy)
		{
			UVoxelMeshComponent* Component = RenderData->MeshComponent.Resolve();
			if (!Component)
			{
				Component = MeshComponents.Num() > 0
					? MeshComponents.Pop()
					: Runtime.NewComponent<UVoxelMeshComponent>();

				RenderData->MeshComponent = Component;
			}
			if (!ensure(Component))
			{
				continue;
			}

			MeshTasks.Add(FMeshTask
			{
				Component,
				RenderData.Get()
			});
		}
		else
		{
			if (UVoxelMeshComponent* Component = RenderData->MeshComponent.Resolve())
			{
				MeshComponents.Add(Component);
			}
		}
	}

	ProcessCollisionTasks();
	ProcessStaticMeshCollisionTasks();
	ProcessNaniteTasks();
	ProcessMeshTasks();
}

void FVoxelRenderComponentCreator::DestroyUnusedComponents()
{
	VOXEL_FUNCTION_COUNTER();

	{
		VOXEL_SCOPE_COUNTER("Destroy MeshComponents");

		for (UVoxelMeshComponent* Component : MeshComponents)
		{
			Component->ClearRenderProxy();
		}

		MeshComponents.ForeachView([&](int32, const TConstVoxelArrayView<UVoxelMeshComponent*> Components)
		{
			Runtime.RemoveComponents(Components);
		});
	}

	{
		VOXEL_SCOPE_COUNTER("Destroy NaniteComponents");

		for (UVoxelNaniteComponent* Component : NaniteComponents)
		{
			Component->ClearMesh();
		}

		NaniteComponents.ForeachView([&](int32, const TConstVoxelArrayView<UVoxelNaniteComponent*> Components)
		{
			Runtime.RemoveComponents(Components);
		});
	}

	{
		VOXEL_SCOPE_COUNTER("Destroy CollisionComponents");

		for (UVoxelCollisionComponent* Component : CollisionComponents)
		{
			Component->ClearCollider();
		}

		CollisionComponents.ForeachView([&](int32, const TConstVoxelArrayView<UVoxelCollisionComponent*> Components)
		{
			Runtime.RemoveComponents(Components);
		});
	}

	{
		VOXEL_SCOPE_COUNTER("Destroy StaticMeshCollisionComponents");

		for (UVoxelStaticMeshCollisionComponent* Component : StaticMeshCollisionComponents)
		{
			Component->SetCollider(nullptr, false);
		}

		StaticMeshCollisionComponents.ForeachView([&](int32, const TConstVoxelArrayView<UVoxelStaticMeshCollisionComponent*> Components)
		{
			Runtime.RemoveComponents(Components);
		});
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelRenderComponentCreator::ProcessCollisionTasks()
{
	VOXEL_FUNCTION_COUNTER_NUM(CollisionTasks.Num());

	for (const FCollisionTask& Task : CollisionTasks)
	{
		UVoxelCollisionComponent& Component = *Task.Component;
		const FVoxelRenderChunkData& RenderData = *Task.RenderData;

		Component.SetCollider(
			RenderData.Collider.ToSharedRef(),
			Config.VisibilityCollision,
			Config.bDoubleSidedCollision,
			GetComponentTransform(RenderData));
	}
}

void FVoxelRenderComponentCreator::ProcessStaticMeshCollisionTasks()
{
	VOXEL_FUNCTION_COUNTER_NUM(StaticMeshCollisionTasks.Num());

	for (const FStaticMeshCollisionTask& Task : StaticMeshCollisionTasks)
	{
		UVoxelStaticMeshCollisionComponent& Component = *Task.Component;
		const FVoxelRenderChunkData& RenderData = *Task.RenderData;

		Component.SetRelativeTransform(GetComponentTransform(RenderData));
		Component.SetBodyInstance(Config.VisibilityCollision);
		Component.SetCollider(RenderData.Collider, Config.bDoubleSidedCollision);

		Component.BodyInstance.SetObjectType(ECC_WorldStatic);
	}
}

void FVoxelRenderComponentCreator::ProcessNaniteTasks()
{
	VOXEL_FUNCTION_COUNTER_NUM(NaniteTasks.Num());

	UMaterialInterface* NaniteWPOMaterial = Subsystem.GetMaterialInstanceRef(EVoxelMegaMaterialTarget::NaniteWPO)->GetMaterial();
	UMaterialInterface* NaniteDisplacementMaterial = Subsystem.GetMaterialInstanceRef(EVoxelMegaMaterialTarget::NaniteDisplacement)->GetMaterial();

	for (const FNaniteTask& Task : NaniteTasks)
	{
		UVoxelNaniteComponent& Component = *Task.Component;
		const FVoxelRenderChunkData& RenderData = *Task.RenderData;

		const bool bEnableTessellation = RenderData.ChunkKey.LOD <= Config.NaniteMaxTessellationLOD;

		Config.ComponentSettings.ApplyToComponent(Component);

		// If tessellation is disabled, we don't need to render any material as this material is only used during Nanite's vertex stage
		Component.SetNaniteMaterial(bEnableTessellation ? NaniteDisplacementMaterial : NaniteWPOMaterial);

		Component.SetRelativeTransform(GetComponentTransform(RenderData));

		Component.SetMesh(
			RenderData.NaniteMesh.ToSharedRef(),
			Config,
			Subsystem.GetNaniteMaterialRenderer());

		FVoxelUtilities::ResetPreviousLocalToWorld(Component);
	}
}

void FVoxelRenderComponentCreator::ProcessMeshTasks()
{
	VOXEL_FUNCTION_COUNTER_NUM(MeshTasks.Num());

	const TSharedRef<FVoxelMaterialRef> NonNaniteMaterial = Subsystem.GetMaterialInstanceRef(EVoxelMegaMaterialTarget::NonNanite);
	const TSharedRef<FVoxelMaterialRef> LumenMaterial = Subsystem.GetMaterialInstanceRef(EVoxelMegaMaterialTarget::Lumen);

	for (const FMeshTask& Task : MeshTasks)
	{
		UVoxelMeshComponent& Component = *Task.Component;
		const FVoxelRenderChunkData& RenderData = *Task.RenderData;

		// Do this now, we need to wait for the buffer pool updates to be done
		Voxel::RenderTask([RenderProxy = RenderData.RenderProxy](FRHICommandListBase& RHICmdList)
		{
			RenderProxy->InitializeVertexFactory_RenderThread(RHICmdList);
		});

		Config.ComponentSettings.ApplyToComponent(Component);

		RenderData.MeshComponentMaterial = NonNaniteMaterial;

		Component.SetRelativeTransform(GetComponentTransform(RenderData));

		Component.SetRenderProxy(
			RenderData.RenderProxy.ToSharedRef(),
			NonNaniteMaterial,
			LumenMaterial);

		FVoxelUtilities::ResetPreviousLocalToWorld(Component);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FTransform FVoxelRenderComponentCreator::GetComponentTransform(const FVoxelRenderChunkData& RenderData) const
{
	return FTransform(
		FQuat::Identity,
		FVector(RenderData.ChunkKey.ChunkKey) * Config.RenderChunkSize * Config.VoxelSize,
		FVector(1 << RenderData.ChunkKey.LOD) * Config.VoxelSize);
}