#include "AnimationVisualizationActor.h"
#include "Engine.h"

namespace GameEngine
{
    bool AnimationVisualizationActor::ParseField(Level * level, CoreLib::Text::TokenReader & parser, bool & isInvalid)
    {
        if (Actor::ParseField(level, parser, isInvalid))
            return true;
        if (parser.LookAhead("mesh"))
        {
            parser.ReadToken();
            MeshName = parser.ReadStringLiteral();
            Mesh = level->LoadMesh(MeshName);
            if (!Mesh)
                isInvalid = false;
            return true;
        }
        if (parser.LookAhead("material"))
        {
            if (parser.NextToken(1).Content == "{")
            {
                MaterialInstance = level->CreateNewMaterial();
                MaterialInstance->Parse(parser);
            }
            else
            {
                parser.ReadToken();
                auto materialName = parser.ReadStringLiteral();
                MaterialInstance = level->LoadMaterial(materialName);
                if (!MaterialInstance)
                    isInvalid = true;
            }
            return true;
        }
        if (parser.LookAhead("Skeleton"))
        {
            parser.ReadToken();
            SkeletonName = parser.ReadStringLiteral();
            Skeleton = level->LoadSkeleton(SkeletonName);
            if (!Skeleton)
                isInvalid = true;
            return true;
        }
        if (parser.LookAhead("MotionGraph"))
        {
            parser.ReadToken();
            MotionGraphName = parser.ReadStringLiteral();
            Graph = level->LoadMotionGraph(MotionGraphName);
            if (!Graph)
                isInvalid = true;
            return true;
        }
        return false;
    }

    void AnimationVisualizationActor::Tick()
    {
        if (Graph)
            nextPose = Graph->States[frameId].Pose;
    }

    void AnimationVisualizationActor::GetDrawables(RendererService * renderService)
    {
        if (!drawable)
            drawable = renderService->CreateSkeletalDrawable(Mesh, Skeleton, MaterialInstance);
        drawable->UpdateTransformUniform(localTransform, nextPose);
        renderService->Add(drawable.Ptr());
    }

    void AnimationVisualizationActor::OnLoad()
    {
        Tick();
    }
}
